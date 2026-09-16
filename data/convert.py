import pandas as pd
import sqlite3

import os
db_path = "data/user/user.db"
if os.path.exists(db_path):
    os.remove(db_path)

conn = sqlite3.connect("data/data.db")

excel_datei = pd.read_excel('data/Mappe1.xlsx', sheet_name=None)

for blatt_name, df in excel_datei.items():
    # 'if_exists="replace"' überschreibt alte Tabellen (oder 'append' zum Anhängen)
    # 'index=False' verhindert, dass die Pandas-Zeilennummern als Spalte gespeichert werden
    df.to_sql(name=blatt_name, con=conn, if_exists="replace", index=False)

conn.close()