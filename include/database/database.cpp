#include "pch.hpp"

#include "database.hpp"
#include "database_config.hpp"

MYSQL *db;

namespace
{
    void ensure_column(const std::string &table, const std::string &column, const std::string &definition)
    {
        const std::string check_query = std::format("SHOW COLUMNS FROM {} LIKE '{}'", table, column);
        if (mysql_query(db, check_query.c_str()))
        {
            std::fprintf(stderr, "[MariaDB] %s\n", mysql_error(db));
            return;
        }

        MYSQL_RES *result = mysql_store_result(db);
        if (!result)
        {
            std::fprintf(stderr, "[MariaDB] %s\n", mysql_error(db));
            return;
        }

        const bool exists = mysql_num_rows(result) != 0;
        mysql_free_result(result);
        if (exists) return;

        const std::string alter_query = std::format("ALTER TABLE {} ADD COLUMN {} {}", table, column, definition);
        if (mysql_query(db, alter_query.c_str()))
            std::fprintf(stderr, "[MariaDB] %s\n", mysql_error(db));
    }

    void ensure_medium_blob(const std::string &table, const std::string &column)
    {
        const std::string check_query = std::format("SHOW COLUMNS FROM {} LIKE '{}'", table, column);
        if (mysql_query(db, check_query.c_str()))
        {
            std::fprintf(stderr, "[MariaDB] %s\n", mysql_error(db));
            return;
        }

        MYSQL_RES *result = mysql_store_result(db);
        if (!result)
        {
            std::fprintf(stderr, "[MariaDB] %s\n", mysql_error(db));
            return;
        }

        MYSQL_ROW row = mysql_fetch_row(result);
        const bool needs_upgrade = row && row[1] && std::string_view(row[1]) == "blob";
        mysql_free_result(result);
        if (!needs_upgrade) return;

        const std::string alter_query = std::format("ALTER TABLE {} MODIFY COLUMN {} MEDIUMBLOB NULL", table, column);
        if (mysql_query(db, alter_query.c_str()))
            std::fprintf(stderr, "[MariaDB] %s\n", mysql_error(db));
    }
}

void create_table_if_not_exist()
{
    std::string query_peer = 
        "CREATE TABLE IF NOT EXISTS peer ("
            "uid INT AUTO_INCREMENT PRIMARY KEY,"
            "growid VARCHAR(18) UNIQUE,"
            "password VARCHAR(18),"
            "role INT NOT NULL DEFAULT 0,"
            "gems INT NOT NULL DEFAULT 0,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
            "inventory BLOB"
        ")";
    std::string query_world = 
        "CREATE TABLE IF NOT EXISTS world ("
            "name VARCHAR(24) NOT NULL PRIMARY KEY,"
            "owner INT DEFAULT 0,"
            "blocks MEDIUMBLOB,"
            "objects MEDIUMBLOB"
        ")";
    if (mysql_query(db, query_peer.c_str()) || mysql_query(db, query_world.c_str()))
    {
        std::fprintf(stderr, "%s\n", mysql_error(db));
    }

    const char *query_world_ban =
        "CREATE TABLE IF NOT EXISTS world_ban ("
            "world_name VARCHAR(24) NOT NULL,"
            "uid INT NOT NULL,"
            "PRIMARY KEY (world_name, uid),"
            "INDEX idx_world_ban_uid (uid)"
        ")";
    if (mysql_query(db, query_world_ban))
        std::fprintf(stderr, "[MariaDB] %s\\n", mysql_error(db));

    ensure_column("peer", "role", "INT NOT NULL DEFAULT 0");
    ensure_column("peer", "gems", "INT NOT NULL DEFAULT 0");
    ensure_column("peer", "level", "INT UNSIGNED NOT NULL DEFAULT 1");
    ensure_column("peer", "xp", "INT UNSIGNED NOT NULL DEFAULT 0");
    ensure_column("peer", "clothing", "BLOB NULL");
    ensure_column("peer", "skin_color", "INT UNSIGNED NOT NULL DEFAULT 2527912447");
    ensure_column("peer", "hair_color", "INT UNSIGNED NOT NULL DEFAULT 16777215");
    ensure_column("peer", "banned", "INT NOT NULL DEFAULT 0");
    ensure_column("peer", "muted_until", "INT UNSIGNED NOT NULL DEFAULT 0");
    ensure_column("world", "minimum_entry_level", "TINYINT UNSIGNED NOT NULL DEFAULT 1");
    ensure_column("world", "access", "BLOB NULL");
    ensure_column("world", "provider_cooldowns", "BLOB NULL");
    ensure_column("world", "vending_machines", "BLOB NULL");
    ensure_column("world", "lock_state", "TINYINT UNSIGNED NOT NULL DEFAULT 0");
    ensure_column("world", "is_public", "BOOLEAN NOT NULL DEFAULT FALSE");
    ensure_medium_blob("world", "blocks");
    ensure_medium_blob("world", "objects");
}

void mysql_connect()
{
    db = mysql_init(NULL);

    if (mysql_real_connect(db, gDb_config.host.c_str(), gDb_config.user.c_str(), gDb_config.passwd.c_str(), NULL, 3306u, NULL, 0ul) == NULL) 
    {
        std::fprintf(stderr, "[MariaDB] %s\n", mysql_error(db));
    }
    else printf("connected to MariaDB server on %s:%d\n", db->host, db->port);

    mysql_query(db, "CREATE DATABASE IF NOT EXISTS gurotopia");
    mysql_select_db(db, "gurotopia");

    create_table_if_not_exist();
}

/* hStmt */

hStmt::hStmt(const std::string &query)
{
    this->pStmt = mysql_stmt_init(db);
    if (!pStmt) 
    {
        std::fprintf(stderr, "%s\n", mysql_error(db));
    }
    if (mysql_stmt_prepare(pStmt, query.c_str(), (u_long)query.size()))
    {
        std::fprintf(stderr, "%s\n", mysql_error(db));
    }
}
hStmt::~hStmt() 
{
    if (mysql_stmt_close(pStmt))
    {
        std::fprintf(stderr, "%s\n", mysql_error(db));
    }
}

void hStmt::bind_param(MYSQL_BIND *param)
{
    if (mysql_stmt_bind_param(pStmt, param)) log_err();
}
void hStmt::execute()
{
    if (mysql_stmt_execute(pStmt)) log_err();
}
void hStmt::fetch()
{
    int value = mysql_stmt_fetch(pStmt);
    if (value == 1 || value == MYSQL_DATA_TRUNCATED) log_err();
    else if (value == 0 || value == MYSQL_NO_DATA) /*@todo do something later...*/; // @note success
}


/* ~ hStmt ~ */


MYSQL_BIND make_bind_in(const signed &buffer)
{
    return { .buffer = (void*)&buffer, .buffer_type = MYSQL_TYPE_LONG };
}
MYSQL_BIND make_bind_in(const unsigned &buffer)
{
    return { .buffer = (void*)&buffer, .buffer_type = MYSQL_TYPE_LONG, .is_unsigned = true };
}
MYSQL_BIND make_bind_in(const long &buffer)
{
    return { .buffer = (void*)&buffer, .buffer_type = MYSQL_TYPE_LONG };
}
MYSQL_BIND make_bind_in(const long long &buffer)
{
    return { .buffer = (void*)&buffer, .buffer_type = MYSQL_TYPE_LONGLONG };
}
MYSQL_BIND make_bind_in(const float &buffer)
{
    return { .buffer = (void*)&buffer, .buffer_type = MYSQL_TYPE_FLOAT };
}
MYSQL_BIND make_bind_in(const std::string &buffer)
{
    return { .buffer = (void*)buffer.c_str(), .buffer_length = (u_long)buffer.size(), .buffer_type = MYSQL_TYPE_STRING };
}
MYSQL_BIND make_bind_in(const std::vector<u_char> &buffer)
{
    return { .buffer = (void*)buffer.data(), .buffer_length = (u_long)buffer.size(), .buffer_type = MYSQL_TYPE_BLOB };
}
MYSQL_BIND make_bind_in(const ::blob &buffer)
{
    return { .buffer = (void*)buffer.data().data(), .buffer_length = (u_long)buffer.size(), .buffer_type = MYSQL_TYPE_BLOB };
}

MYSQL_BIND make_bind_out(signed &buffer)
{
    return { .buffer = &buffer, .buffer_type = MYSQL_TYPE_LONG };
}
MYSQL_BIND make_bind_out(unsigned &buffer)
{
    return { .buffer = &buffer, .buffer_type = MYSQL_TYPE_LONG, .is_unsigned = true };
}
MYSQL_BIND make_bind_out(long &buffer)
{
    return { .buffer = &buffer, .buffer_type = MYSQL_TYPE_LONG };
}
MYSQL_BIND make_bind_out(long long &buffer)
{
    return { .buffer = &buffer, .buffer_type = MYSQL_TYPE_LONGLONG };
}
MYSQL_BIND make_bind_out(float &buffer)
{
    return { .buffer = &buffer, .buffer_type = MYSQL_TYPE_FLOAT };
}
MYSQL_BIND make_bind_out(std::string &buffer)
{
    buffer.resize(1024, '\0');

    return { .buffer = buffer.data(), .buffer_length = (u_long)buffer.size(), .buffer_type = MYSQL_TYPE_STRING };
}
MYSQL_BIND make_bind_out(std::vector<u_char> &buffer)
{
    buffer.resize(cord(0, 60)* sizeof(::block));

    return { .buffer = buffer.data(), .buffer_length = (u_long)buffer.size(), .buffer_type = MYSQL_TYPE_BLOB };
}
MYSQL_BIND make_bind_out(::blob &buffer)
{
    buffer.resize(cord(0, 60)* sizeof(::block));

    return { .buffer = buffer.data().data(), .buffer_length = (u_long)buffer.size(), .buffer_type = MYSQL_TYPE_BLOB };
}
