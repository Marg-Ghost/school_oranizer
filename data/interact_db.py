import sqlite3
from pathlib import Path
from typing import Any


DB_PATH = Path(__file__).resolve().parent / "data.db"


def _connection() -> sqlite3.Connection:
	connection = sqlite3.connect(DB_PATH)
	connection.row_factory = sqlite3.Row
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
			"SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name"
		).fetchall()
	return [row[0] for row in rows]


def get_rows(table_name: str) -> list[dict[str, Any]]:
	with _connection() as connection:
		_table_columns(connection, table_name)
		rows = connection.execute(f'SELECT * FROM "{table_name}"').fetchall()
	return [dict(row) for row in rows]


def add_row(table_name: str, values: dict[str, Any]) -> dict[str, Any]:
	with _connection() as connection:
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
	try:
		test_grade = float(grade)
	except ValueError as error:
		raise ValueError("Die Testnote muss eine Zahl sein") from error

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
		grades = connection.execute(
			'SELECT * FROM "grades" WHERE "subject" = ?', (subject,)
		).fetchone()
		if grades is None:
			connection.execute(
				'INSERT INTO "grades" ("subject", "oral", "written", "general") VALUES (?, ?, ?, ?)',
				(subject, "", grade, ""),
			)
		else:
			oral = grades["oral"]
			try:
				general = (float(oral) + test_grade) / 2
			except (TypeError, ValueError):
				general = ""
			connection.execute(
				'UPDATE "grades" SET "written" = ?, "general" = ? WHERE "subject" = ?',
				(grade, f"{general:.2f}" if general != "" else "", subject),
			)
		connection.commit()
		updated_test = connection.execute(
			'SELECT * FROM "test" WHERE "id" = ?', (test_id,)
		).fetchone()
	return dict(updated_test)
