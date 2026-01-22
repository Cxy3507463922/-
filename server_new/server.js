const express = require('express');
const bodyParser = require('body-parser');
const path = require('path');
const config = require('./config');
const db = require('./database');
const cors = require('cors');

const app = express();
app.use(cors());
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));
app.use(express.static(path.join(__dirname, 'public')));

// 全局变量状态
let currentStatus = {
    device_connected: false,
    motion_detected: false,
    relay_active: false,
    last_update: Date.now() / 1000,
    last_motion_time: Date.now() / 1000,
    countdown_active: false,
    countdown_start: 0,
    countdown_duration: 3 * 60, // 3分钟
    pending_command: null,
    forced_mode: false,
    forced_release_armed: false // 无人后置 armed，下一次有人再退出强制
};

// 将当前状态转换为状态键，便于历史记录
function getStateKey(status) {
    if (!status.device_connected) return 'OFFLINE';
    if (status.motion_detected) return 'SOMEONE'; // 优先显示有人状态
    if (status.forced_mode) return 'FORCED';
    if (status.relay_active && status.countdown_active) return 'COUNTDOWN';
    return 'IDLE';
}

const STATE_NAMES = {
    'OFFLINE': '设备离线',
    'SOMEONE': '有人',
    'FORCED': '强制开灯',
    'IDLE': '无人',
    'COUNTDOWN': '即将关灯'
};

function logStateChange(oldState, newState, deviceId) {
    if (oldState === newState) return;

    const timestamp = Date.now() / 1000;
    let level = 'INFO';
    let message = `状态变更: ${STATE_NAMES[oldState] || oldState} -> ${STATE_NAMES[newState] || newState}`;

    if (newState === 'OFFLINE') {
        level = 'ERROR';
        message = '状态更新：设备离线';
    } else if (newState === 'FORCED') {
        level = 'WARN';
        message = '状态更新：强制开灯';
    } else if (newState === 'COUNTDOWN') {
        level = 'WARN';
        message = '状态更新：即将关灯';
    } else if (newState === 'SOMEONE') {
        level = 'INFO';
        message = '状态更新：有人';
    } else if (newState === 'IDLE') {
        level = 'INFO';
        message = '状态更新：无人关灯';
    }

    db.log(deviceId || config.DEVICE_ID, level, message, timestamp);
}

// 检查倒计时逻辑
setInterval(() => {
    const oldState = getStateKey(currentStatus);
    if (currentStatus.countdown_active) {
        const elapsed = (Date.now() / 1000) - currentStatus.countdown_start;
        const remainingTime = Math.max(0, currentStatus.countdown_duration - elapsed);
        
        if (remainingTime <= 0) {
            currentStatus.countdown_active = false;
            currentStatus.relay_active = false;
            currentStatus.forced_mode = false;
            currentStatus.pending_command = 'relay_off';
        }
    }
    logStateChange(oldState, getStateKey(currentStatus), config.DEVICE_ID);
}, 1000);

// 心跳超时：超过 60 秒未上报则标记离线
setInterval(() => {
    const oldState = getStateKey(currentStatus);
    const now = Date.now() / 1000;
    if (currentStatus.device_connected && (now - currentStatus.last_update > 60)) {
        currentStatus.device_connected = false;
        currentStatus.countdown_active = false;
        currentStatus.pending_command = null;
    }
    logStateChange(oldState, getStateKey(currentStatus), config.DEVICE_ID);
}, 5000);

// API 端点
app.get('/api/v1/full_status', (req, res) => {
    const elapsed = currentStatus.countdown_active ? (Date.now() / 1000) - currentStatus.countdown_start : 0;
    const remaining_time = Math.max(0, currentStatus.countdown_duration - elapsed);

    // 防止数据库死锁导致前端无响应
    let responseSent = false;
    const sendResponse = (data) => {
        if (!responseSent) {
            responseSent = true;
            res.json(data);
        }
    };

    // 设置 1.5秒 超时，如果数据库没反应，直接返回当前状态
    const dbTimeout = setTimeout(() => {
        console.error('Database timeout - returning partial status');
        sendResponse({
            status: currentStatus,
            remaining_time: remaining_time,
            devices: [],
            logs: [{
                level: 'ERROR', 
                message: '数据库无响应 (Check guardian.db file)', 
                timestamp: Date.now() / 1000
            }]
        });
    }, 1500);

    try {
        db.getAllDevices((err, devices) => {
            if (err) console.error('DB Error (devices):', err);
            
            // 获取两部分日志：1. 所有的最近 500 条（含 DEBUG）；2. 非 DEBUG 的最近 100 条
            db.getRecentLogs(null, 500, (err, allLogs) => {
                if (err) console.error('DB Error (allLogs):', err);

                db.getRecentLogsFiltered(null, 100, 'DEBUG', (err, infoLogs) => {
                    if (err) console.error('DB Error (infoLogs):', err);

                    // 返回最近约 24 小时历史（28800 条）
                    db.getStatusHistory(28800, (err, history) => {
                        if (err) console.error('DB Error (history):', err);
                        
                        clearTimeout(dbTimeout);
                        sendResponse({
                            status: currentStatus,
                            remaining_time: remaining_time,
                            devices: devices || [],
                            logs: allLogs || [],
                            info_logs: infoLogs || [],
                            history: history || []
                        });
                    });
                });
            });
        });
    } catch (e) {
        clearTimeout(dbTimeout);
        console.error('Server Logic Error:', e);
        res.status(500).json({ error: 'Internal Server Error' });
    }
});

app.post('/api/v1/status', (req, res) => {
    const data = req.body;
    const deviceId = data.device_id || config.DEVICE_ID;
    
    // 支持直接传 situation 代码 (1:有人, 2:没人但灯亮, 3:没人且灯暗)
    let motion = !!data.motion;
    let relay = !!data.relay;
    
    if (data.situation === 1) {
        motion = true;
        relay = true;
    } else if (data.situation === 2) {
        motion = false;
        relay = true;
    } else if (data.situation === 3) {
        motion = false;
        relay = false;
    }
    
    // 硬件上报的三种情况判定
    let reportText = motion ? "有人" : (relay ? "无人但灯亮" : "无人且灯暗");
    db.log(deviceId, 'DEBUG', `硬件上报: ${reportText} (${deviceId})`);

    const oldState = getStateKey(currentStatus);

    currentStatus.device_connected = true;
    currentStatus.last_update = Date.now() / 1000;
    
    // 【修改】核心逻辑变动：硬件上报的 relay 状态仅作为“回显”参考，不再覆盖服务器的决策。
    // 服务器根据逻辑（有人/倒计时/强制模式）维护自己的 relay_active，
    // 硬件通过 /api/v1/relay_state 接口同步执行结果。
    
    if (motion !== currentStatus.motion_detected) {
        if (motion) {
            // 如果之前无人且强制模式已 armed，则此刻退出强制
            if (currentStatus.forced_mode && currentStatus.forced_release_armed) {
                currentStatus.forced_mode = false;
                currentStatus.forced_release_armed = false;
            }

            currentStatus.motion_detected = true;
            currentStatus.last_motion_time = Date.now() / 1000;
            currentStatus.relay_active = true;
            currentStatus.countdown_active = false;
            currentStatus.pending_command = 'relay_on';
        } else {
            currentStatus.motion_detected = false;
            // 强制模式：不倒计时，但标记下次有人时退出
            if (currentStatus.forced_mode) {
                currentStatus.forced_release_armed = true;
            } else {
                // 只有在灯亮着的情况下离开，才进入即将关灯倒计时
                if (currentStatus.relay_active) {
                    currentStatus.countdown_active = true;
                    currentStatus.countdown_start = Date.now() / 1000;
                } else {
                    currentStatus.countdown_active = false;
                }
            }
        }
    }

    const newState = getStateKey(currentStatus);
    logStateChange(oldState, newState, deviceId);

    db.updateDeviceStatus(deviceId, motion ? 'motion_detected' : 'idle');
    if (motion) db.setPendingAction(deviceId, true);

    res.json({ success: true });
});

app.get('/api/v1/command', (req, res) => {
    const command = currentStatus.pending_command;
    if (command) {
        currentStatus.pending_command = null;
    }
    res.json({
        device_id: config.DEVICE_ID,
        command: command,
        timestamp: Date.now() / 1000
    });
});

// 极简继电器状态接口：1 为开灯，0 为关灯
app.get('/api/v1/relay_state', (req, res) => {
    const state = getStateKey(currentStatus);
    res.send(state === 'IDLE' ? "0" : "1");
});

app.post('/api/v1/command', (req, res) => {
    const { command } = req.body;
    if (!command) return res.status(400).json({ success: false, message: 'Missing command' });

    const oldState = getStateKey(currentStatus);

    if (command === 'keep_power') {
        currentStatus.forced_mode = true;
        // 如果当前已经是无人状态，则直接标记为“已处于无人过”，这样下次有人来就会自动解锁
        currentStatus.forced_release_armed = !currentStatus.motion_detected;
        currentStatus.countdown_active = false;
        currentStatus.relay_active = true;
        currentStatus.pending_command = 'relay_on';
    } else if (command === 'cancel_forced') {
        currentStatus.forced_mode = false;
        currentStatus.forced_release_armed = false;
        // 取消后，如果是有人状态则保持开灯，如果是无人状态则恢复倒计时或关灯
        if (currentStatus.motion_detected) {
            currentStatus.relay_active = true;
            currentStatus.countdown_active = false;
        } else {
            // 无人时取消，直接触发关灯或倒计时逻辑
            currentStatus.countdown_active = true;
            currentStatus.countdown_start = Date.now() / 1000;
        }
    } else if (command === 'power_off') {
        currentStatus.forced_mode = false;
        currentStatus.forced_release_armed = false;
        currentStatus.countdown_active = false;
        currentStatus.relay_active = false;
        currentStatus.pending_command = 'relay_off';
    } else if (command === 'reset') {
        currentStatus.motion_detected = false;
        currentStatus.relay_active = false;
        currentStatus.countdown_active = false;
        currentStatus.pending_command = null;
        currentStatus.forced_mode = false;
        currentStatus.forced_release_armed = false;
    } else {
        return res.status(400).json({ success: false, message: 'Unknown command' });
    }

    logStateChange(oldState, getStateKey(currentStatus), config.DEVICE_ID);

    res.json({ success: true, message: `Command executed: ${command}` });
});

// 启动
db.waitForReady().then(() => {
    // 启动一分钟采样，记录过去24小时状态轨迹
    const sampleAndStore = () => {
        const stateKey = getStateKey(currentStatus);
        db.addStatusSample(stateKey);
    };

    // 立即采样一次，后续每 3 秒采样
    sampleAndStore();
    setInterval(sampleAndStore, 3 * 1000);

    app.listen(config.PORT, () => {
        db.updateDeviceStatus(config.DEVICE_ID, 'idle');
        db.log('SYSTEM', 'INFO', '服务器已启动');
        console.log(`Server running at http://localhost:${config.PORT}`);
    });
}).catch(err => {
    console.error('Failed to initialize database:', err);
    process.exit(1);
});
