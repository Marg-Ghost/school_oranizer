import sqlite3
from pathlib import Path
from typing import Any


DB_PATH = Path(__file__).resolve().parent / "data.db"


def _connection() -> sqlite3.Connection:
	connection = sqlite3.connect(DB_PATH)
	connection.row_factory = sqlite3.Row
	connection.execute(
		'''CREATE TABLE IF NOT EXISTS "grade_entries" (
			"id" INTEGER PRIMARY KEY AUTOINCREMENT,
			"subject" TEXT NOT NULL,
			"type" TEXT NOT NULL CHECK ("type" IN ('oral', 'written')),
			"value" REAL NOT NULL,
			"date" TEXT,
			"source_id" TEXT
		)'''
	)
	connection.execute(
		'''CREATE TABLE IF NOT EXISTS "schema_meta" (
			"key" TEXT PRIMARY KEY,
			"value" TEXT NOT NULL
		)'''
	)
	_migrated = connection.execute(
		'SELECT 1 FROM "schema_meta" WHERE "key" = "grades_migrated"'
	).fetchone()
	if _migrated is None:
		_old_columns = {
			row[1]
			for row in connection.execute('PRAGMA table_info("grades")').fetchall()
		}
		if {"subject", "oral", "written"}.issubset(_old_columns):
			_old_grades = connection.execute(
				'SELECT "subject", "oral", "written" FROM "grades"'
			).fetchall()
			for old_grade in _old_grades:
				for grade_type in ("oral", "written"):
					try:
						value = float(old_grade[grade_type])
					except (TypeError, ValueError):
						continue
					connection.execute(
						'INSERT INTO "grade_entries" ("subject", "type", "value") VALUES (?, ?, ?)',
						(old_grade["subject"], grade_type, value),
					)
		_old_test_columns = {
			row[1]
			for row in connection.execute('PRAGMA table_info("test")').fetchall()
		}
		if {"subject", "grade"}.issubset(_old_test_columns):
			_old_tests = connection.execute(
				'SELECT "id", "subject", "grade" FROM "test" WHERE "grade" IS NOT NULL AND "grade" != ""'
			).fetchall()
			for old_test in _old_tests:
				value = _number(old_test["grade"])
				if value is not None:
					connection.execute(
						'INSERT INTO "grade_entries" ("subject", "type", "value", "source_id") VALUES (?, ?, ?, ?)',
						(old_test["subject"], "written", value, old_test["id"]),
					)
		connection.execute(
			'INSERT INTO "schema_meta" ("key", "value") VALUES ("grades_migrated", "1")'
		)
	_test_columns = {
		row[1]
		for row in connection.execute('PRAGMA table_info("test")').fetchall()
	}
	if _test_columns:
		if "done" not in _test_columns:
			connection.execute('ALTER TABLE "test" ADD COLUMN "done" TEXT DEFAULT "0"')
		if "grade" not in _test_columns:
			connection.execute('ALTER TABLE "test" ADD COLUMN "grade" TEXT')
		connection.commit()
	return connection


def _table_columns(connection: sqlite3.Connection, table_name: str) -> list[str]:
	tables = connection.execute(
		"SELECT name FROM sqlite_master WHERE type = 'table'"
	).fetchall()
	if table_name not in {table[0] for table in tables}:
		raise ValueError(f"Unbekannte Tabelle: {table_name}")

	return [row[1] for row in connection.execute(f'PRAGMA table_info("{table_name}")')]


def list_tables() -> list[str]:
	with _connection() as connection:
		rows = connection.execute(
			"SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT IN ('grade_entries', 'schema_meta') ORDER BY name"
		).fetchall()
	return [row[0] for row in rows]


def _number(value: Any) -> float | None:
	try:
		return float(value)
	except (TypeError, ValueError):
		return None


def _grade_summaries(connection: sqlite3.Connection) -> list[dict[str, Any]]:
	subjects = connection.execute(
		'SELECT DISTINCT "subject" FROM "grade_entries" ORDER BY "subject"'
	).fetchall()
	result = []
	for (subject,) in subjects:
		averages = {}
		for grade_type in ("oral", "written"):
			averages[grade_type] = connection.execute(
				'SELECT AVG("value") FROM "grade_entries" WHERE "subject" = ? AND "type" = ?',
				(subject, grade_type),
			).fetchone()[0]
		oral = averages["oral"]
		written = averages["written"]
		if oral is None:
			general = written
		elif written is None:
			general = oral
		else:
			general = (oral + written * 2) / 3
		result.append({
			"subject": subject,
			"oral_avg": round(oral, 2) if oral is not None else None,
			"written_avg": round(written, 2) if written is not None else None,
			"general": round(general, 2) if general is not None else None,
		})
	return result


def get_rows(
	table_name: str, sort_by: str | None = None, desc: bool = False
) -> list[dict[str, Any]]:
	with _connection() as connection:
		if table_name == "grades":
			rows = _grade_summaries(connection)
			allowed = {"subject", "oral_avg", "written_avg", "general"}
			if sort_by and sort_by not in allowed:
				raise ValueError(f"Unbekannte Spalte zum Sortieren: {sort_by}")
			if sort_by:
				rows.sort(key=lambda row: (row[sort_by] is None, row[sort_by]), reverse=desc)
			return rows

		columns = _table_columns(connection, table_name)
		if sort_by:
			if sort_by not in columns:
				raise ValueError(f"Unbekannte Spalte zum Sortieren: {sort_by}")
			order = "DESC" if desc else "ASC"
			order_clause = f' ORDER BY "{sort_by}" {order}'
		else:
			order_clause = ""
		rows = connection.execute(f'SELECT * FROM "{table_name}"{order_clause}').fetchall()
	return [dict(row) for row in rows]


def add_row(table_name: str, values: dict[str, Any]) -> dict[str, Any]:
	with _connection() as connection:
		if table_name == "grades":
			if "subject" not in values:
				raise ValueError("Das Fach muss angegeben werden")
			for grade_type in ("oral", "written"):
				if grade_type in values and values[grade_type] != "":
					value = _number(values[grade_type])
					if value is None:
						raise ValueError(f"{grade_type} muss eine Zahl sein")
					connection.execute(
						'INSERT INTO "grade_entries" ("subject", "type", "value", "date") VALUES (?, ?, ?, ?)',
						(values["subject"], grade_type, value, values.get("date")),
					)
			connection.commit()
			return next(
				(row for row in _grade_summaries(connection) if row["subject"] == values["subject"]),
				{"subject": values["subject"]},
			)
		columns = _table_columns(connection, table_name)
		unknown_columns = set(values) - set(columns)
		if unknown_columns:
			raise ValueError(f"Unbekannte Spalten: {', '.join(sorted(unknown_columns))}")
		if not values:
			raise ValueError("Mindestens eine Spalte muss angegeben werden")
		values = dict(values)
		if table_name == "grades" and "oral" in values and "written" in values:
			try:
				values["general"] = f"{(float(values['oral']) + float(values['written'])) / 2:.2f}"
			except (TypeError, ValueError):
				values["general"] = ""

		column_names = list(values)
		placeholders = ", ".join("?" for _ in column_names)
		quoted_columns = ", ".join(f'"{column}"' for column in column_names)
		cursor = connection.execute(
			f'INSERT INTO "{table_name}" ({quoted_columns}) VALUES ({placeholders})',
			[values[column] for column in column_names],
		)
		connection.commit()

		if "id" in columns:
			row = connection.execute(
				f'SELECT * FROM "{table_name}" WHERE "id" = ?',
				(values.get("id", cursor.lastrowid),),
			).fetchone()
			if row is not None:
				return dict(row)
	return values


def update_row(table_name: str, row_id: str, values: dict[str, Any]) -> dict[str, Any]:
	with _connection() as connection:
		columns = _table_columns(connection, table_name)
		if "id" not in columns:
			raise ValueError(f"Tabelle {table_name} besitzt keine id-Spalte")
		unknown_columns = set(values) - (set(columns) - {"id"})
		if unknown_columns:
			raise ValueError(f"Unbekannte Spalten: {', '.join(sorted(unknown_columns))}")
		if not values:
			raise ValueError("Mindestens eine Spalte muss angegeben werden")

		assignments = ", ".join(f'"{column}" = ?' for column in values)
		cursor = connection.execute(
			f'UPDATE "{table_name}" SET {assignments} WHERE "id" = ?',
			[*values.values(), row_id],
		)
		connection.commit()
		if cursor.rowcount == 0:
			raise KeyError(f"Kein Datensatz mit id {row_id}")
		row = connection.execute(
			f'SELECT * FROM "{table_name}" WHERE "id" = ?', (row_id,)
		).fetchone()
	return dict(row)


def delete_row(table_name: str, row_id: str) -> None:
	with _connection() as connection:
		columns = _table_columns(connection, table_name)
		if "id" not in columns:
			raise ValueError(f"Tabelle {table_name} besitzt keine id-Spalte")
		cursor = connection.execute(
			f'DELETE FROM "{table_name}" WHERE "id" = ?', (row_id,)
		)
		connection.commit()
		if cursor.rowcount == 0:
			raise KeyError(f"Kein Datensatz mit id {row_id}")


def record_test_grade(test_id: str, grade: str) -> dict[str, Any]:
	test_grade = _number(grade)
	if test_grade is None:
		raise ValueError("Die Testnote muss eine Zahl sein")

	with _connection() as connection:
		test = connection.execute(
			'SELECT * FROM "test" WHERE "id" = ?', (test_id,)
		).fetchone()
		if test is None:
			raise KeyError(f"Kein Test mit id {test_id}")

		connection.execute(
			'UPDATE "test" SET "grade" = ?, "done" = ? WHERE "id" = ?',
			(grade, "1", test_id),
		)
		subject = test["subject"]
		previous_grade = connection.execute(
			'SELECT "id" FROM "grade_entries" WHERE "type" = "written" AND "source_id" = ?',
			(test_id,),
		).fetchone()
		if previous_grade is None:
			connection.execute(
				'INSERT INTO "grade_entries" ("subject", "type", "value", "source_id") VALUES (?, ?, ?, ?)',
				(subject, "written", test_grade, test_id),
			)
		else:
			connection.execute(
				'UPDATE "grade_entries" SET "value" = ?, "subject" = ? WHERE "id" = ?',
				(test_grade, subject, previous_grade["id"]),
			)
		connection.commit()
		updated_test = connection.execute(
			'SELECT * FROM "test" WHERE "id" = ?', (test_id,)
		).fetchone()
	return dict(updated_test)
