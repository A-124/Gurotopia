#include "pch.hpp"
#include <fstream>
#include <sstream>
#include "custom_content.hpp"
namespace custom_content {
namespace { std::unordered_map<int,custom_item> custom_items; std::unordered_map<int,recipe> recipes;
std::vector<std::string> split(const std::string& s,char d){std::vector<std::string> out;std::stringstream ss(s);std::string v;while(std::getline(ss,v,d))out.push_back(v);return out;}
bool integer(const std::string& s,int& v){try{size_t p{};v=std::stoi(s,&p);return p==s.size();}catch(...){return false;}}
}
bool reload(){
 std::ifstream f("resources/custom_items.txt"); if(!f) return false;
 std::unordered_map<int,custom_item> next_items; std::unordered_map<int,recipe> next_recipes;
 std::string line;
 while(std::getline(f,line)){
  if(line.empty()||line[0]=='#') continue;
  auto p=split(line,'|'); if(p.empty()) continue;
  if(p[0]=="item" && p.size()>=6){ custom_item x; if(!integer(p[1],x.id)||!integer(p[3],x.base_item)||!integer(p[4],x.type)||!integer(p[5],x.rarity)) continue; x.name=p[2]; if(p.size()>6)x.tradeable=(p[6]!="0"); if(x.id<0||x.id>65535)continue; next_items[x.id]=std::move(x); }
  else if(p[0]=="recipe" && p.size()>=4){ recipe r; if(!integer(p[1],r.result)||!integer(p[2],r.amount))continue; auto ingredients=split(p[3],','); bool ok=true; for(auto& i:ingredients){auto q=split(i,':');int id{},amount{};if(q.size()!=2||!integer(q[0],id)||!integer(q[1],amount)||amount<=0){ok=false;break;}r.ingredients.emplace_back(id,amount);}if(ok&&!r.ingredients.empty())next_recipes[r.result]=std::move(r); }
 }
 custom_items.swap(next_items); recipes.swap(next_recipes); return true;
}
const custom_item* find_item(int id) noexcept {auto i=custom_items.find(id);return i==custom_items.end()?nullptr:&i->second;}
bool is_custom_item(int id) noexcept{return custom_items.contains(id);}
const recipe* find_recipe(int result) noexcept{auto i=recipes.find(result);return i==recipes.end()?nullptr:&i->second;}
const std::unordered_map<int,custom_item>& items() noexcept{return custom_items;}
}
