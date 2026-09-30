// test_sqlite3.cp — the vendored build has foreign keys ON by default,
// still honors PRAGMA foreign_keys = OFF, and enforces FK rules.
#include <sqlite3.h>
#include <cstdio>
#include <cstring>

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static int pragma_int(sqlite3* db, const char* sql) {
    sqlite3_stmt* s = nullptr; int v = -1;
    if (sqlite3_prepare_v2(db, sql, -1, &s, nullptr) == SQLITE_OK && sqlite3_step(s) == SQLITE_ROW)
        v = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
    return v;
}

int main() {
    CHECK(std::strcmp(sqlite3_libversion(), SQLITE_VERSION) == 0);   // header matches library
    sqlite3* db = nullptr;
    CHECK(sqlite3_open(":memory:", &db) == SQLITE_OK);
    CHECK(pragma_int(db, "PRAGMA foreign_keys") == 1);               // default ON

    sqlite3_exec(db, "CREATE TABLE p(id INTEGER PRIMARY KEY);"
                     "CREATE TABLE c(id INTEGER PRIMARY KEY, p INTEGER REFERENCES p(id));", nullptr, nullptr, nullptr);
    CHECK(sqlite3_exec(db, "INSERT INTO c(p) VALUES(99)", nullptr, nullptr, nullptr) == SQLITE_CONSTRAINT);

    sqlite3_exec(db, "PRAGMA foreign_keys = OFF", nullptr, nullptr, nullptr);
    CHECK(pragma_int(db, "PRAGMA foreign_keys") == 0);               // still switchable
    CHECK(sqlite3_exec(db, "INSERT INTO c(p) VALUES(99)", nullptr, nullptr, nullptr) == SQLITE_OK);

    CHECK(sqlite3_compileoption_used("ENABLE_FTS5") == 1);
    sqlite3_close(db);
    std::printf("%s (sqlite %s)\n", failures ? "FAILED" : "ok", sqlite3_libversion());
    return failures ? 1 : 0;
}
