"""Exercise the actual SQL strings in db.c when a C toolchain is unavailable.

This checks the persistence statements, not the native C glue or Win32 UI.
"""
import ast
import pathlib
import re
import sqlite3
import tempfile
import unittest

SOURCE = (pathlib.Path(__file__).resolve().parents[1] / "src/db.c").read_text()
STRINGS = [
    "".join(ast.literal_eval(x) for x in re.findall(r'"(?:[^"\\]|\\.)*"', group))
    for group in re.findall(r'(?:"(?:[^"\\]|\\.)*"\s*)+', SOURCE)
]


def statement(prefix):
    return next(s for s in STRINGS if s.startswith(prefix))


class LedgerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = pathlib.Path(self.temp.name) / "test.db"
        self.db = sqlite3.connect(self.path, isolation_level=None)
        self.db.executescript(statement("PRAGMA journal_mode"))

    def tearDown(self):
        self.db.close()
        self.temp.cleanup()

    def insert(self, stamp=120, peak_down=100, peak_up=25, off_down=50, off_up=10):
        self.db.execute(statement("INSERT INTO usage"), (stamp, peak_down, peak_up, off_down, off_up))

    def totals(self, start, end):
        return self.db.execute(statement("SELECT coalesce(sum(pd)"), (start, end)).fetchone()

    def test_upsert_and_half_open_cycle(self):
        self.insert()
        self.insert()
        self.insert(180)
        self.assertEqual(self.totals(120, 180), (200, 50, 100, 20))
        self.assertEqual(self.totals(180, 240), (100, 25, 50, 10))

    def test_corrections_preserve_raw_traffic(self):
        self.insert()
        self.db.execute(statement("INSERT INTO corrections"), (150, -125, -60))
        self.assertEqual(self.totals(120, 180), (100, 25, 50, 10))
        self.assertEqual(self.db.execute(statement("SELECT coalesce(sum(p),"), (120, 180)).fetchone(), (-125, -60))
        self.assertEqual(self.db.execute(statement("SELECT coalesce(sum(p),"), (180, 240)).fetchone(), (0, 0))

    def test_rollback_retry_and_reopen(self):
        self.db.execute("BEGIN IMMEDIATE")
        self.insert()
        self.db.execute("ROLLBACK")
        self.assertEqual(self.totals(120, 180), (0, 0, 0, 0))
        self.db.execute("BEGIN IMMEDIATE")
        self.insert()
        self.db.execute("COMMIT")
        self.db.close()
        self.db = sqlite3.connect(self.path, isolation_level=None)
        self.assertEqual(self.totals(120, 180), (100, 25, 50, 10))

    def test_session_update_and_export_order(self):
        q = statement("INSERT INTO sessions")
        self.db.execute(q, (100, 120, 1000))
        self.db.execute(q, (100, 180, 61000))
        self.assertEqual(self.db.execute("SELECT * FROM sessions").fetchall(), [(100, 180, 61000)])
        self.insert(180)
        self.db.execute(statement("INSERT INTO corrections"), (150, 5, 6))
        rows = self.db.execute(statement("SELECT t,pd,pu")).fetchall()
        self.assertEqual(rows[0], (150, 0, 0, 0, 0, 5, 6))
        self.assertEqual(rows[1], (180, 100, 25, 50, 10, 0, 0))


if __name__ == "__main__":
    unittest.main(verbosity=2)
