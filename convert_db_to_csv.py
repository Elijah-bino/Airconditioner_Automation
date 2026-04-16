import pandas as pd
import sqlite3

conn = sqlite3.connect("ac_logs.db")
df = pd.read_sql_query("SELECT * FROM ac_logs", conn)
df.to_csv("ac_logs.csv", index=False)
conn.close()