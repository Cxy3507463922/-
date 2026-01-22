const sqlite3 = require('sqlite3').verbose();
const fs = require('fs');
const path = require('path');
const config = require('./config');

class DatabaseManager {
    constructor() {
        // 确保数据库目录存在
        const dbDir = path.dirname(config.DATABASE_PATH);
        if (dbDir && dbDir !== '.') {
            fs.mkdirSync(dbDir, { recursive: true });
        }

        this.dbReady = new Promise((resolve, reject) => {
            this.db = new sqlite3.Database(config.DATABASE_PATH, (err) => {
                if (err) {
                    console.error('Error opening database', err);
                    reject(err);
                } else {
                    console.log('Database connected, initializing tables...');
                    this.initDatabase(resolve, reject);
                }
            });
        });
    }

    initDatabase(resolve, reject) {
        this.db.serialize(() => {
            // Devices table
            this.db.run(`
                CREATE TABLE IF NOT EXISTS devices (
                    device_id TEXT PRIMARY KEY,
                    status TEXT NOT NULL,
                    last_seen REAL NOT NULL,
                    pending_action INTEGER DEFAULT 0,
                    last_decision TEXT,
                    created_time REAL NOT NULL
                )
            `);

            // Decisions table
            this.db.run(`
                CREATE TABLE IF NOT EXISTS decisions (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    device_id TEXT NOT NULL,
                    decision TEXT NOT NULL,
                    note TEXT,
                    timestamp REAL NOT NULL
                )
            `);

            // Logs table
            this.db.run(`
                CREATE TABLE IF NOT EXISTS logs (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    device_id TEXT NOT NULL,
                    level TEXT NOT NULL,
                    message TEXT NOT NULL,
                    timestamp REAL NOT NULL
                )
            `);

            // Status history table (per-minute sampling)
            this.db.run(`
                CREATE TABLE IF NOT EXISTS status_history (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    ts REAL NOT NULL,
                    state TEXT NOT NULL
                )
            `, (err) => {
                if (err) {
                    console.error('Error creating tables:', err);
                    if (reject) reject(err);
                } else {
                    console.log('All tables ready');
                    if (resolve) resolve();
                }
            });
        });
    }

    waitForReady() {
        return this.dbReady;
    }

    addStatusSample(state, tsSeconds = Date.now() / 1000) {
        this.waitForReady().then(() => {
            this.db.run('INSERT INTO status_history (ts, state) VALUES (?, ?)', [tsSeconds, state], (err) => {
                if (err) console.error('addStatusSample error:', err);
            });
            // 保留最近 24 小时数据（每 3s 一条，28800 条 = 24h）
            this.db.run('DELETE FROM status_history WHERE id NOT IN (SELECT id FROM status_history ORDER BY ts DESC LIMIT 28800)', (err) => {
                if (err) console.error('prune status_history error:', err);
            });
        }).catch((err) => console.error('DB not ready for addStatusSample:', err));
    }

    getStatusHistory(limit, callback) {
        this.waitForReady().then(() => {
            this.db.all('SELECT ts, state FROM status_history ORDER BY ts DESC LIMIT ?', [limit], callback);
        }).catch((err) => {
            console.error('DB not ready for getStatusHistory:', err);
            callback(err);
        });
    }

    updateDeviceStatus(deviceId, status) {
        const now = Date.now() / 1000;
        this.waitForReady().then(() => {
            this.db.run(`
                INSERT INTO devices (device_id, status, last_seen, created_time)
                VALUES (?, ?, ?, ?)
                ON CONFLICT(device_id) DO UPDATE SET
                    status = excluded.status,
                    last_seen = excluded.last_seen
            `, [deviceId, status, now, now], (err) => {
                if (err) console.error('updateDeviceStatus error:', err);
            });
        }).catch((err) => console.error('DB not ready for updateDeviceStatus:', err));
    }

    getDeviceStatus(deviceId, callback) {
        this.waitForReady().then(() => {
            this.db.get('SELECT * FROM devices WHERE device_id = ?', [deviceId], callback);
        }).catch((err) => {
            console.error('DB not ready for getDeviceStatus:', err);
            callback(err);
        });
    }

    getAllDevices(callback) {
        this.waitForReady().then(() => {
            this.db.all('SELECT * FROM devices', [], callback);
        }).catch((err) => {
            console.error('DB not ready for getAllDevices:', err);
            callback(err);
        });
    }

    log(deviceId, level, message) {
        const now = Date.now() / 1000;
        this.waitForReady().then(() => {
            this.db.run('INSERT INTO logs (device_id, level, message, timestamp) VALUES (?, ?, ?, ?)',
                [deviceId, level, message, now], (err) => {
                    if (err) console.error('log insert error:', err);
                });
        }).catch((err) => console.error('DB not ready for log():', err));
    }

    getRecentLogs(deviceId, limit, callback) {
        this.waitForReady().then(() => {
            if (deviceId) {
                this.db.all('SELECT * FROM logs WHERE device_id = ? ORDER BY timestamp DESC LIMIT ?',
                    [deviceId, limit], callback);
            } else {
                this.db.all('SELECT * FROM logs ORDER BY timestamp DESC LIMIT ?',
                    [limit], callback);
            }
        }).catch((err) => {
            console.error('DB not ready for getRecentLogs:', err);
            callback(err);
        });
    }

    getRecentLogsFiltered(deviceId, limit, excludeLevel, callback) {
        this.waitForReady().then(() => {
            let sql = 'SELECT * FROM logs WHERE level != ?';
            let params = [excludeLevel];
            if (deviceId) {
                sql += ' AND device_id = ?';
                params.push(deviceId);
            }
            sql += ' ORDER BY timestamp DESC LIMIT ?';
            params.push(limit);
            this.db.all(sql, params, callback);
        }).catch((err) => {
            console.error('DB not ready for getRecentLogsFiltered:', err);
            callback(err);
        });
    }

    recordDecision(deviceId, decision, note) {
        const now = Date.now() / 1000;
        this.waitForReady().then(() => {
            this.db.run('INSERT INTO decisions (device_id, decision, note, timestamp) VALUES (?, ?, ?, ?)',
                [deviceId, decision, note, now], (err) => {
                    if (err) console.error('recordDecision insert error:', err);
                });
            this.db.run('UPDATE devices SET last_decision = ? WHERE device_id = ?', [decision, deviceId], (err) => {
                if (err) console.error('recordDecision update error:', err);
            });
        }).catch((err) => console.error('DB not ready for recordDecision:', err));
    }

    getRecentDecisions(deviceId, limit, callback) {
        this.waitForReady().then(() => {
            this.db.all('SELECT * FROM decisions WHERE device_id = ? ORDER BY timestamp DESC LIMIT ?',
                [deviceId, limit], callback);
        }).catch((err) => {
            console.error('DB not ready for getRecentDecisions:', err);
            callback(err);
        });
    }

    setPendingAction(deviceId, pending) {
        this.waitForReady().then(() => {
            this.db.run('UPDATE devices SET pending_action = ? WHERE device_id = ?', [pending ? 1 : 0, deviceId], (err) => {
                if (err) console.error('setPendingAction error:', err);
            });
        }).catch((err) => console.error('DB not ready for setPendingAction:', err));
    }
}

module.exports = new DatabaseManager();
