import sqlite3
import time
from datetime import datetime


class DatabaseManager:
    def __init__(self, db_path='guardian.db'):
        self.db_path = db_path
        self.init_database()

    def init_database(self):
        """初始化数据库表结构"""
        conn = self.get_connection()
        cursor = conn.cursor()

        # 设备状态表
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS devices (
                device_id TEXT PRIMARY KEY,
                status TEXT NOT NULL,
                last_seen REAL NOT NULL,
                pending_action INTEGER DEFAULT 0,
                last_decision TEXT,
                created_time REAL NOT NULL
            )
        ''')

        # 决策记录表
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS decisions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id TEXT NOT NULL,
                decision TEXT NOT NULL,
                decision_time REAL NOT NULL,
                note TEXT,
                FOREIGN KEY (device_id) REFERENCES devices (device_id)
            )
        ''')

        # 系统日志表
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS logs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id TEXT,
                level TEXT NOT NULL,
                message TEXT NOT NULL,
                timestamp REAL NOT NULL
            )
        ''')

        conn.commit()
        conn.close()
        self.log("SYSTEM", "INFO", "数据库初始化完成")

    def get_connection(self):
        """获取数据库连接"""
        return sqlite3.connect(self.db_path)

    def update_device_status(self, device_id, status):
        """更新设备状态"""
        conn = self.get_connection()
        cursor = conn.cursor()

        current_time = time.time()
        cursor.execute('''
            INSERT OR REPLACE INTO devices 
            (device_id, status, last_seen, created_time) 
            VALUES (?, ?, ?, ?)
        ''', (device_id, status, current_time, current_time))

        conn.commit()
        conn.close()
        self.log(device_id, "INFO", f"设备状态更新: {status}")

    def set_pending_action(self, device_id, pending=True):
        """设置待处理动作标志"""
        conn = self.get_connection()
        cursor = conn.cursor()

        cursor.execute('''
            UPDATE devices SET pending_action = ? WHERE device_id = ?
        ''', (1 if pending else 0, device_id))

        conn.commit()
        conn.close()
        self.log(device_id, "INFO", f"设置待处理动作: {pending}")

    def record_decision(self, device_id, decision, note=""):
        """记录用户决策"""
        conn = self.get_connection()
        cursor = conn.cursor()

        current_time = time.time()
        cursor.execute('''
            INSERT INTO decisions (device_id, decision, decision_time, note)
            VALUES (?, ?, ?, ?)
        ''', (device_id, decision, current_time, note))

        # 更新设备最后决策
        cursor.execute('''
            UPDATE devices SET last_decision = ?, pending_action = 0 
            WHERE device_id = ?
        ''', (decision, device_id))

        conn.commit()
        conn.close()
        self.log(device_id, "INFO", f"用户决策记录: {decision} - {note}")

    def log(self, device_id, level, message):
        """记录系统日志"""
        conn = self.get_connection()
        cursor = conn.cursor()

        current_time = time.time()
        cursor.execute('''
            INSERT INTO logs (device_id, level, message, timestamp)
            VALUES (?, ?, ?, ?)
        ''', (device_id, level, message, current_time))

        conn.commit()
        conn.close()

    def get_device_status(self, device_id):
        """获取设备状态"""
        conn = self.get_connection()
        cursor = conn.cursor()

        cursor.execute('''
            SELECT * FROM devices WHERE device_id = ?
        ''', (device_id,))

        result = cursor.fetchone()
        conn.close()
        return result

    def get_all_devices(self):
        """获取所有设备列表"""
        conn = self.get_connection()
        cursor = conn.cursor()

        cursor.execute('SELECT * FROM devices')
        results = cursor.fetchall()
        conn.close()
        return results

    def get_recent_decisions(self, device_id=None, limit=10):
        """获取最近的决策记录"""
        conn = self.get_connection()
        cursor = conn.cursor()

        if device_id:
            cursor.execute('''
                SELECT * FROM decisions WHERE device_id = ? 
                ORDER BY decision_time DESC LIMIT ?
            ''', (device_id, limit))
        else:
            cursor.execute('''
                SELECT * FROM decisions 
                ORDER BY decision_time DESC LIMIT ?
            ''', (limit,))

        results = cursor.fetchall()
        conn.close()
        return results

    def get_recent_logs(self, device_id=None, limit=20):
        """获取最近的系统日志"""
        conn = self.get_connection()
        cursor = conn.cursor()

        if device_id:
            cursor.execute('''
                SELECT * FROM logs WHERE device_id = ? 
                ORDER BY timestamp DESC LIMIT ?
            ''', (device_id, limit))
        else:
            cursor.execute('''
                SELECT * FROM logs 
                ORDER BY timestamp DESC LIMIT ?
            ''', (limit,))

        results = cursor.fetchall()
        conn.close()
        return results