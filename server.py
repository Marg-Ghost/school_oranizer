from typing import Any

import uvicorn
from fastapi import FastAPI, HTTPException
from fastapi.responses import PlainTextResponse

import data.interact_db as interact_db

app = FastAPI(title="CLI Organizer API")


@app.get("/tables")
def tables() -> list[str]:
	return interact_db.list_tables()


@app.get("/tables/{table_name}")
def rows(
	table_name: str, sort_by: str | None = None, desc: bool = False
) -> list[dict[str, Any]]:
	try:
		return interact_db.get_rows(table_name, sort_by=sort_by, desc=desc)
	except ValueError as error:
		raise HTTPException(status_code=400, detail=str(error)) from error


@app.get("/tables/{table_name}/view", response_class=PlainTextResponse)
def table_view(
	table_name: str, sort_by: str | None = None, desc: bool = False
) -> str:
	try:
		rows = interact_db.get_rows(table_name, sort_by=sort_by, desc=desc)
	except ValueError as error:
		raise HTTPException(status_code=400, detail=str(error)) from error
	if not rows:
		return "Keine Einträge vorhanden."

	columns = list(rows[0])
	widths = {
		column: max(len(column), *(len(str(row.get(column, ""))) for row in rows))
		for column in columns
	}
	header = " | ".join(column.ljust(widths[column]) for column in columns)
	separator = "-+-".join("-" * widths[column] for column in columns)
	lines = [header, separator]
	for row in rows:
		lines.append(" | ".join(str(row.get(column, "")).ljust(widths[column]) for column in columns))
	return "\n".join(lines)


@app.post("/tables/{table_name}", status_code=201)
def create_row(table_name: str, values: dict[str, Any]) -> dict[str, Any]:
	try:
		return interact_db.add_row(table_name, values)
	except ValueError as error:
		raise HTTPException(status_code=400, detail=str(error)) from error


@app.patch("/tables/{table_name}/{row_id}")
def change_row(
	table_name: str, row_id: str, values: dict[str, Any]
) -> dict[str, Any]:
	try:
		return interact_db.update_row(table_name, row_id, values)
	except KeyError as error:
		raise HTTPException(status_code=404, detail=str(error)) from error
	except ValueError as error:
		raise HTTPException(status_code=400, detail=str(error)) from error


@app.delete("/tables/{table_name}/{row_id}", status_code=204)
def remove_row(table_name: str, row_id: str) -> None:
	try:
		interact_db.delete_row(table_name, row_id)
	except KeyError as error:
		raise HTTPException(status_code=404, detail=str(error)) from error
	except ValueError as error:
		raise HTTPException(status_code=400, detail=str(error)) from error


@app.post("/tests/{test_id}/grade")
def grade_test(test_id: str, values: dict[str, Any]) -> dict[str, Any]:
	if "grade" not in values:
		raise HTTPException(status_code=400, detail="Das Feld grade fehlt")
	try:
		return interact_db.record_test_grade(test_id, str(values["grade"]))
	except KeyError as error:
		raise HTTPException(status_code=404, detail=str(error)) from error
	except ValueError as error:
		raise HTTPException(status_code=400, detail=str(error)) from error


if __name__ == "__main__":
	uvicorn.run(app, host="0.0.0.0", port=4301)