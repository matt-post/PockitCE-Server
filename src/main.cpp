#include <httplib.h>
#include <sqlite3.h>
#include <lua.hpp>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <cstdlib>

const std::string red = "\033[31m";
const std::string green = "\033[32m";
const std::string blue = "\033[34m";
const std::string reset = "\033[0m";

struct Block {
    std::string type;
    std::string name;
    int x, y;
    std::map<std::string, std::string> properties;
};

std::vector<Block> board{
    {"slider", "slider1", 0, 0, {{"value", "0"}}},
    {"button", "button1", 1, 0, {{"pressed", "false"}}},
    {"knob", "knob1", 2, 0, {{"angle", "0"}}},
    {"led", "led1", 0, 1, {{"state", "off"}}},
    {"eink", "eink1", 1, 1, {{"text", "Hello"}}},
    {"buzzer", "buzzer1", 2, 1, {{"state", "silent"}}}
};

std::string getLocalIP() {
    char buffer[128];
    std::string result = "";
    FILE* pipe = popen("hostname -I | awk '{print $1}'", "r");
    if (!pipe) {
        return "127.0.0.1";
    }
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    result.erase(result.find_last_not_of(" \n\r\t") + 1);
    return result;
}

void initializeDatabase() {
    sqlite3* db;
    char* errMsg = nullptr;

    int rc = sqlite3_open("apps.db", &db);
    if (rc != SQLITE_OK) {
        std::cerr << red << "Cannot open database: " << sqlite3_errmsg(db) << reset << std::endl;
        sqlite3_close(db);
        exit(1);
    }

    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS Apps (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            script TEXT NOT NULL
        );
    )";

    rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << red << "SQL error: " << errMsg << reset << std::endl;
        sqlite3_free(errMsg);
    }

    sqlite3_close(db);
    std::cout << green << "Database initialized successfully." << reset << std::endl;
}

bool saveScriptToDatabase(const std::string& name, const std::string& script) {
    sqlite3* db;

    int rc = sqlite3_open("apps.db", &db);
    if (rc != SQLITE_OK) {
        std::cerr << red << "Cannot open database: " << sqlite3_errmsg(db) << reset << std::endl;
        sqlite3_close(db);
        return false;
    }

    const char* sql = "INSERT INTO Apps (name, script) VALUES (?, ?);";
    sqlite3_stmt* stmt;

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << red << "Failed to prepare statement: " << sqlite3_errmsg(db) << reset << std::endl;
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, script.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << red << "Failed to execute statement: " << sqlite3_errmsg(db) << reset << std::endl;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return rc == SQLITE_DONE;
}

int luaopen_pockitce(lua_State* L) {
    lua_newtable(L);

    lua_pushcfunction(L, [](lua_State* L) -> int {
        int x = lua_tointeger(L, 1);
        int y = lua_tointeger(L, 2);
        const char* property = lua_tostring(L, 3);
        const char* value = lua_tostring(L, 4);

        for (auto& block : board) {
            if (block.x == x && block.y == y) {
                block.properties[property] = value;
                lua_pushstring(L, "Property updated");
                return 1;
            }
        }
        lua_pushstring(L, "Block not found");
        return 1;
    });
    lua_setfield(L, -2, "updateBlock");

    lua_pushcfunction(L, [](lua_State* L) -> int {
        lua_newtable(L);
        for (size_t i = 0; i < board.size(); ++i) {
            lua_pushinteger(L, i + 1);
            lua_newtable(L);

            lua_pushstring(L, "type"); lua_pushstring(L, board[i].type.c_str()); lua_settable(L, -3);
            lua_pushstring(L, "name"); lua_pushstring(L, board[i].name.c_str()); lua_settable(L, -3);
            lua_pushstring(L, "x"); lua_pushinteger(L, board[i].x); lua_settable(L, -3);
            lua_pushstring(L, "y"); lua_pushinteger(L, board[i].y); lua_settable(L, -3);

            lua_newtable(L);
            for (const auto& prop : board[i].properties) {
                lua_pushstring(L, prop.first.c_str());
                lua_pushstring(L, prop.second.c_str());
                lua_settable(L, -3);
            }
            lua_setfield(L, -2, "properties");

            lua_settable(L, -3);
        }
        return 1;
    });
    lua_setfield(L, -2, "getBlocks");

    lua_pushcfunction(L, [](lua_State* L) -> int {
        const char* name = lua_tostring(L, 1);

        for (auto& block : board) {
            if (block.name == name) {
                lua_newtable(L);

                lua_pushstring(L, "type"); lua_pushstring(L, block.type.c_str()); lua_settable(L, -3);
                lua_pushstring(L, "name"); lua_pushstring(L, block.name.c_str()); lua_settable(L, -3);
                lua_pushstring(L, "x"); lua_pushinteger(L, block.x); lua_settable(L, -3);
                lua_pushstring(L, "y"); lua_pushinteger(L, block.y); lua_settable(L, -3);

                lua_newtable(L);
                for (const auto& prop : block.properties) {
                    lua_pushstring(L, prop.first.c_str());
                    lua_pushstring(L, prop.second.c_str());
                    lua_settable(L, -3);
                }
                lua_setfield(L, -2, "properties");

                return 1;
            }
        }

        lua_pushnil(L);
        return 1;
    });
    lua_setfield(L, -2, "getBlockByName");

    lua_pushcfunction(L, [](lua_State* L) -> int {
        const char* name = lua_tostring(L, 1);
        const char* property = lua_tostring(L, 2);
        const char* value = lua_tostring(L, 3);

        for (auto& block : board) {
            if (block.name == name) {
                block.properties[property] = value;
                lua_pushstring(L, "Property updated");
                return 1;
            }
        }

        lua_pushstring(L, "Block not found");
        return 1;
    });
    lua_setfield(L, -2, "updateBlockByName");

    return 1;
}

void runLuaScript(const std::string& script) {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    luaL_requiref(L, "pockitce", luaopen_pockitce, 1);
    lua_pop(L, 1);

    if (luaL_dostring(L, script.c_str())) {
        std::cerr << red << "Lua Error: " << lua_tostring(L, -1) << reset << std::endl;
        lua_pop(L, 1);
    }

    lua_close(L);
}

int main() {
    initializeDatabase();

    httplib::Server svr;

    svr.Post("/upload", [](const httplib::Request& req, httplib::Response& res) {
        auto body = req.body;

        std::string name, script;
        size_t name_start = body.find("\"name\":\"") + 8;
        size_t name_end = body.find("\"", name_start);
        name = body.substr(name_start, name_end - name_start);

        size_t script_start = body.find("\"script\":\"") + 10;
        size_t script_end = body.find_last_of("\"");
        script = body.substr(script_start, script_end - script_start);

        if (saveScriptToDatabase(name, script)) {
            res.status = 200;
            res.set_content("Script saved successfully.", "text/plain");
        } else {
            res.status = 500;
            res.set_content("Failed to save script.", "text/plain");
        }
    });

    svr.Post("/run", [](const httplib::Request& req, httplib::Response& res) {
        auto body = req.body;
        size_t script_start = body.find("\"script\":\"") + 10;
        size_t script_end = body.find_last_of("\"");
        std::string script = body.substr(script_start, script_end - script_start);

        runLuaScript(script);
        res.status = 200;
        res.set_content("Script executed. Check server logs for output.", "text/plain");
    });

    svr.Get("/test_lua", [](const httplib::Request& req, httplib::Response& res) {
        runLuaScript(R"(
            local pockitce = require("pockitce")
            print("Initial board state:")
            local blocks = pockitce.getBlocks()
            for i, block in ipairs(blocks) do
                print(block.type, block.name, "at", block.x, block.y)
                for prop, val in pairs(block.properties) do
                    print("  " .. prop .. ": " .. val)
                end
            end
            pockitce.updateBlock(0, 0, "value", "42")
            print("Updated block at (0,0):")
            local updatedBlocks = pockitce.getBlocks()
            print(updatedBlocks[1].properties.value)
        )");
        res.set_content("Lua test script executed. Check logs for outputs.", "text/plain");
    });

    {std::string ip = getLocalIP();
    std::cout << green << "PockitCE server started on port 8080 on IP " << ip << reset << std::endl;}
    std::cout << blue << "Credit to the PockitCE Team 2024, All Rights Reserved" << reset << std::endl;
    std::cout << red << "Hello from Canada - mattpost" << reset << std::endl;

    svr.listen("0.0.0.0", 8080);
    return 0;
}