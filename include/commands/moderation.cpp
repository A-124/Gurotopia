#include "pch.hpp"
#include <charconv>
#include <limits>
#include "moderation.hpp"
#include "action/join_request.hpp"
#include "action/quit_to_exit.hpp"
#include "onVariant/ConsoleMessage.hpp"

namespace {
void say(ENetEvent &e, std::string s) { if (e.peer) on::ConsoleMessage(e.peer, std::move(s)); }
bool parse_num(std::string_view s, int max, int &v) {
 if (s.empty()) return false;
 auto [end, ec] = std::from_chars(s.data(), s.data()+s.size(), v);
 return ec == std::errc{} && end == s.data()+s.size() && v > 0 && v <= max;
}
std::string_view args(std::string_view s) {
 auto p=s.find_first_of(" \t"); if(p==s.npos) return {};
 s.remove_prefix(p); p=s.find_first_not_of(" \t"); return p==s.npos ? std::string_view{} : s.substr(p);
}
bool split(std::string_view s,std::string_view &a,std::string_view &b) {
 auto p=s.find_first_of(" \t"); if(p==s.npos) return false;
 a=s.substr(0,p); s.remove_prefix(p); p=s.find_first_not_of(" \t");
 if(p==s.npos) return false; b=s.substr(p); return !a.empty();
}
int uid_for(std::string_view who) {
 if(!db || who.empty()) return 0;
 int n{}; bool numeric=parse_num(who,std::numeric_limits<int>::max(),n); std::string name(who);
 ::hStmt st{numeric ? "SELECT uid FROM peer WHERE uid = ? OR LOWER(growid)=LOWER(?) LIMIT 1" : "SELECT uid FROM peer WHERE LOWER(growid)=LOWER(?) LIMIT 1"};
 if(numeric) { MYSQL_BIND p[2]={make_bind_in(n),make_bind_in(name)}; st.bind_param(p); }
 else { MYSQL_BIND p=make_bind_in(name); st.bind_param(&p); }
 st.execute(); if(mysql_stmt_store_result(st.pStmt)!=0 || !mysql_stmt_num_rows(st.pStmt)) return 0;
 int uid{}; MYSQL_BIND out=make_bind_out(uid); mysql_stmt_bind_result(st.pStmt,&out); st.fetch(); return uid;
}
std::string name_for(int uid) {
 ::hStmt st{"SELECT growid FROM peer WHERE uid=? LIMIT 1"}; MYSQL_BIND p=make_bind_in(uid); st.bind_param(&p); st.execute();
 if(mysql_stmt_store_result(st.pStmt)!=0 || !mysql_stmt_num_rows(st.pStmt)) return {};
 std::string name; u_long len{}; MYSQL_BIND out=make_bind_out(name); out.length=&len; mysql_stmt_bind_result(st.pStmt,&out); st.fetch(); name.resize(len); return name;
}
std::pair<ENetPeer*,::peer*> online(int uid) {
 for(auto *c:peers()) if(c&&c->data) { auto *p=static_cast<::peer*>(c->data); if(p->user_id==uid) return {c,p}; } return {};
}
::world *here(::peer *p) {
 if(!p||p->recent_worlds.back().empty()) return nullptr;
 auto i=std::ranges::find(worlds,p->recent_worlds.back(),&::world::name); return i==worlds.end()?nullptr:&*i;
}
bool access_to(const ::peer &p,const ::world &w) {
 return p.role==DEVELOPER || w.owner==p.user_id || std::ranges::find(w.access,p.user_id)!=w.access.end();
}
bool world_ban(std::string_view world,int uid,bool ban) {
 std::string name(world);
 if(ban) { ::hStmt s{"INSERT INTO world_ban (world_name,uid) VALUES (?,?) ON DUPLICATE KEY UPDATE uid=VALUES(uid)"}; MYSQL_BIND p[2]={make_bind_in(name),make_bind_in(uid)}; s.bind_param(p); s.execute(); return mysql_stmt_errno(s.pStmt)==0; }
 ::hStmt s{"DELETE FROM world_ban WHERE world_name=? AND uid=?"}; MYSQL_BIND p[2]={make_bind_in(name),make_bind_in(uid)}; s.bind_param(p); s.execute(); return mysql_stmt_errno(s.pStmt)==0;
}
bool server_ban(int uid,bool ban) {
 signed v=ban?1:0; ::hStmt s{"UPDATE peer SET banned=? WHERE uid=?"}; MYSQL_BIND p[2]={make_bind_in(v),make_bind_in(uid)}; s.bind_param(p); s.execute(); return mysql_stmt_errno(s.pStmt)==0;
}
void to_start(ENetPeer *c) { if(!c||!c->data) return; ENetEvent e{.peer=c}; action::quit_to_exit(e,"",true); action::join_request(e,"","START"); }
void server_kick(ENetEvent &e,ENetPeer *c,::peer *p) {
 on::ConsoleMessage(c,"`4You were kicked from the server by a developer.``");
 ENetEvent fake{.peer=c}; action::quit_to_exit(fake,"",true); enet_peer_disconnect(c,0);
 say(e,std::format("`2Kicked {} from the server.``",p->growid));
}
}
std::string username_for_uid(int uid) {
 if(uid<=0) return {};
 for(auto *c:peers()) if(c&&c->data) { auto *p=static_cast<::peer*>(c->data); if(p->user_id==uid && !p->growid.empty()) return p->growid; }
 return db ? name_for(uid) : std::string{};
}
bool is_world_banned(std::string_view world,int uid) {
 if(!db||world.empty()||uid<=0) return false; std::string name(world);
 ::hStmt s{"SELECT 1 FROM world_ban WHERE world_name=? AND uid=? LIMIT 1"}; MYSQL_BIND p[2]={make_bind_in(name),make_bind_in(uid)}; s.bind_param(p); s.execute();
 return mysql_stmt_store_result(s.pStmt)==0 && mysql_stmt_num_rows(s.pStmt)>0;
}
void command_setlevel(ENetEvent &e,std::string_view text) {
 auto *actor=e.peer?static_cast<::peer*>(e.peer->data):nullptr;
 if(!actor||actor->role!=DEVELOPER){say(e,"`4Only developers can use /setlevel.``");return;}
 std::string_view who,level_s; auto sp=text.find_first_of(" \t");
 if(sp==text.npos||!split(text.substr(sp+1),who,level_s)){say(e,"`oUsage: /setlevel <player|UID> <level 1-125>``");return;}
 int level{}; if(!parse_num(level_s,125,level)){say(e,"`4Level must be from 1 to 125.``");return;}
 int uid=uid_for(who); if(!uid){say(e,std::format("`4Player '{}' not found.``",who));return;}
 if(uid==actor->user_id){say(e,"`4You cannot change your own level.``");return;}
 auto [c,p]=online(uid);
 if(p){p->level[0]=static_cast<u_int>(level);p->level[1]=0;p->save_progress();}
 else {::hStmt s{"UPDATE peer SET level=?,xp=0 WHERE uid=?"}; unsigned v=static_cast<unsigned>(level); MYSQL_BIND b[2]={make_bind_in(v),make_bind_in(uid)};s.bind_param(b);s.execute();}
 say(e,std::format("`2Set {} (UID {}) to level {}.``",p?p->growid:name_for(uid),uid,level));
 if(c&&p) on::ConsoleMessage(c,std::format("`2Your level was set to {} by a developer.``",level));
}
void command_kick(ENetEvent &e,std::string_view text) {
 auto *a=e.peer?static_cast<::peer*>(e.peer->data):nullptr;if(!a)return; auto who=args(text);
 if(who.empty()){say(e,"`oUsage: /kick <player|UID>``");return;} int uid=uid_for(who);
 if(!uid){say(e,"`4Player not found.``");return;} auto [c,p]=online(uid);
 if(!c||!p){say(e,"`4That player is not online.``");return;} if(p==a){say(e,"`4You cannot kick yourself.``");return;}
 if(a->role==DEVELOPER){server_kick(e,c,p);return;}
 auto *w=here(a); if(!w||a->netid==0||p->recent_worlds.back()!=w->name||!access_to(*a,*w)){say(e,"`4You need world owner/access permission and the target must be in your world.``");return;}
 auto name=p->growid; to_start(c); say(e,std::format("`2Kicked {} from world {}.``",name,w->name));
}
void command_ban(ENetEvent &e,std::string_view text) {
 auto *a=e.peer?static_cast<::peer*>(e.peer->data):nullptr;if(!a)return;auto who=args(text);
 if(who.empty()){say(e,"`oUsage: /ban <player|UID>``");return;}int uid=uid_for(who);
 if(!uid){say(e,"`4Player not found.``");return;}auto [c,p]=online(uid);if(p==a){say(e,"`4You cannot ban yourself.``");return;}
 if(a->role==DEVELOPER){
  if(!server_ban(uid,true)){say(e,"`4Server ban failed.``");return;}
  if(p&&c){p->banned=true;p->save_moderation();on::ConsoleMessage(c,"`4You were banned from the server.``");ENetEvent f{.peer=c};action::quit_to_exit(f,"",true);enet_peer_disconnect(c,0);}
  say(e,std::format("`2Server-banned {} (UID {}).``",p?p->growid:name_for(uid),uid));return;
 }
 auto *w=here(a);if(!w||a->netid==0||!access_to(*a,*w)){say(e,"`4You need owner/access permission in this world to ban.``");return;}
 if(!world_ban(w->name,uid,true)){say(e,"`4World ban failed.``");return;}
 if(p&&c&&p->recent_worlds.back()==w->name)to_start(c);
 say(e,std::format("`2Banned {} from world {}.``",p?p->growid:name_for(uid),w->name));
}
void command_unban(ENetEvent &e,std::string_view text) {
 auto *a=e.peer?static_cast<::peer*>(e.peer->data):nullptr;if(!a)return;auto who=args(text);
 if(who.empty()){say(e,"`oUsage: /unban <player|UID>``");return;}int uid=uid_for(who);
 if(!uid){say(e,"`4Player not found.``");return;}
 if(a->role==DEVELOPER){if(!server_ban(uid,false)){say(e,"`4Server unban failed.``");return;}say(e,std::format("`2Server-unbanned {} (UID {}).``",name_for(uid),uid));return;}
 auto *w=here(a);if(!w||a->netid==0||!access_to(*a,*w)){say(e,"`4You need owner/access permission in this world to unban.``");return;}
 if(!world_ban(w->name,uid,false)){say(e,"`4World unban failed.``");return;}
 say(e,std::format("`2Unbanned {} from world {}.``",name_for(uid),w->name));
}
void command_pull(ENetEvent &e,std::string_view text) {
 auto *a=e.peer?static_cast<::peer*>(e.peer->data):nullptr;if(!a||a->netid==0){say(e,"`4Enter a world before using /pull.``");return;}auto who=args(text);
 if(who.empty()){say(e,"`oUsage: /pull <player|UID>``");return;}int uid=uid_for(who);
 if(!uid){say(e,"`4Player not found.``");return;}auto [c,p]=online(uid);
 if(!c||!p){say(e,"`4That player is not online.``");return;}if(p==a){say(e,"`4You cannot pull yourself.``");return;}
 const std::string dest=a->recent_worlds.back();
 if(a->role!=DEVELOPER){
  auto *w=here(a);if(!w||!access_to(*a,*w)||p->recent_worlds.back()!=dest){say(e,"`4You need world owner/access permission; you cannot pull from another world.``");return;}
  p->pos=a->pos;send_varlist(c,{"OnSetPos",CL_Vec2f{p->pos.x,p->pos.y}},p->netid);say(e,std::format("`2Pulled {} to you.``",p->growid));on::ConsoleMessage(c,std::format("`2You were pulled by {}.``",a->growid));return;
 }
 if(p->recent_worlds.back()!=dest)to_start(c);
 ENetEvent fake{.peer=c};action::join_request(fake,"",dest);p->pos=a->pos;
 send_varlist(c,{"OnSetPos",CL_Vec2f{p->pos.x,p->pos.y}},p->netid);
 say(e,std::format("`2Pulled {} to you.``",p->growid));on::ConsoleMessage(c,std::format("`2A developer pulled you to {}.``",dest));
}
