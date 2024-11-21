#include <httplib.h>
#include <sqlite3.h>
#include <lua.hpp>
#include <iostream>
#include <string>
#include <fstream>

void initializeDatabase() {
    sqlite3* db;
    char* errMsg = nullptr;

    int rc = sqlite3_open("apps.db", &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
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
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }

    sqlite3_close(db);
}

bool saveScriptToDatabase(const std::string& name, const std::string& script) {
    sqlite3* db;

    int rc = sqlite3_open("apps.db", &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    const char* sql = "INSERT INTO Apps (name, script) VALUES (?, ?);";
    sqlite3_stmt* stmt;

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, script.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Failed to execute statement: " << sqlite3_errmsg(db) << std::endl;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return rc == SQLITE_DONE;
}

int luaopen_pockitce(lua_State* L) {
    lua_newtable(L);

    lua_newtable(L);
    lua_newtable(L);
    lua_pushstring(L, "slider action!"); lua_setfield(L, -2, "doSomething");
    lua_setfield(L, -2, "slider");

    lua_newtable(L);
    lua_pushstring(L, "button pressed!"); lua_setfield(L, -2, "press");
    lua_setfield(L, -2, "button");

    lua_newtable(L);
    lua_pushstring(L, "knob turned!"); lua_setfield(L, -2, "turn");
    lua_setfield(L, -2, "knob");

    lua_setfield(L, -2, "input");

    lua_newtable(L);
    lua_newtable(L);
    lua_pushstring(L, "LED toggled!"); lua_setfield(L, -2, "toggle");
    lua_setfield(L, -2, "led");

    lua_newtable(L);
    lua_pushstring(L, "eInk updated!"); lua_setfield(L, -2, "update");
    lua_setfield(L, -2, "eink");

    lua_setfield(L, -2, "display");

    lua_newtable(L);
    lua_newtable(L);
    lua_pushstring(L, "Buzzer sound!"); lua_setfield(L, -2, "buzz");
    lua_setfield(L, -2, "buzzer");

    lua_setfield(L, -2, "sound");

    lua_setfield(L, -2, "blocks");

    return 1;
}

int main() {
    initializeDatabase();
    
    httplib::Server svr;

    // Upload route for saving script to the database
    svr.Post("/upload", [](const httplib::Request& req, httplib::Response& res) {
        auto body = req.body;

        // Parse the JSON payload manually
        std::string name, script;
        if (body.find("name") == std::string::npos || body.find("script") == std::string::npos) {
            res.status = 400;
            res.set_content("Invalid request payload.", "text/plain");
            return;
        }

        size_t name_start = body.find("\"name\":\"") + 8;
        size_t name_end = body.find("\"", name_start);
        name = body.substr(name_start, name_end - name_start);

        size_t script_start = body.find("\"script\":\"") + 10;
        size_t script_end = body.find("\"", script_start);
        script = body.substr(script_start, script_end - script_start);

        if (saveScriptToDatabase(name, script)) {
            res.status = 200;
            res.set_content("Script saved successfully.", "text/plain");
        } else {
            res.status = 500;
            res.set_content("Failed to save script.", "text/plain");
        }
    });

    // Test route for lua script execution
    svr.Get("/test_pockitce", [](const httplib::Request& req, httplib::Response& res) {
        lua_State* L = luaL_newstate();
        luaL_openlibs(L);

        luaL_requiref(L, "pockitce", luaopen_pockitce, 1);
        lua_pop(L, 1);

        luaL_dostring(L, R"(
            local pockitce = require("pockitce")
            print(pockitce.blocks.input.slider.doSomething)
            print(pockitce.blocks.display.led.toggle)
            print(pockitce.blocks.sound.buzzer.buzz)
        )");

        lua_close(L);
        res.status = 200;
        res.set_content("pockitce module tested. Check logs for output.", "text/plain");
    });

    svr.listen("localhost", 8080);
    return 0;
}
