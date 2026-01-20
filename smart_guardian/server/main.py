from flask import Flask, render_template, request, jsonify
import time
import config
from database import DatabaseManager

# 初始化 Flask 应用
app = Flask(__name__)
app.config.from_object(config)

# 初始化数据库
db = DatabaseManager(config.DATABASE_PATH)

# 添加自定义过滤器
def format_time(timestamp, format_str='%Y-%m-%d %H:%M:%S'):
    """格式化时间戳为可读时间"""
    return time.strftime(format_str, time.localtime(timestamp))

app.jinja_env.filters['strftime'] = format_time

# 全局变量
current_status = {
    'device_connected': False,
    'motion_detected': False,
    'alarm_active': False,
    'last_update': time.time()
}

@app.route('/')
def index():
    """主页面"""
    devices = db.get_all_devices()
    logs = db.get_recent_logs(limit=10)
    return render_template('index.html', status=current_status, devices=devices, logs=logs)

@app.route('/status')
def status():
    """状态页面"""
    device = db.get_device_status(config.DEVICE_ID)
    logs = db.get_recent_logs(config.DEVICE_ID, limit=20)
    return render_template('status.html', device=device, logs=logs, current_status=current_status, time=time)

@app.route('/action', methods=['GET', 'POST'])
def action():
    """动作处理页面"""
    device = db.get_device_status(config.DEVICE_ID)
    decisions = db.get_recent_decisions(config.DEVICE_ID, limit=10)
    
    if request.method == 'POST':
        decision = request.form.get('decision')
        note = request.form.get('note', '')
        
        if decision:
            db.record_decision(config.DEVICE_ID, decision, note)
            # 根据决策执行相应操作
            if decision == 'alarm_on':
                current_status['alarm_active'] = True
                db.log(config.DEVICE_ID, 'WARNING', '手动触发警报')
            elif decision == 'alarm_off':
                current_status['alarm_active'] = False
                db.log(config.DEVICE_ID, 'INFO', '手动关闭警报')
            elif decision == 'reset':
                current_status['motion_detected'] = False
                current_status['alarm_active'] = False
                db.log(config.DEVICE_ID, 'INFO', '系统重置')
    
    return render_template('action.html', device=device, decisions=decisions, current_status=current_status)

# API 端点
@app.route('/api/v1/status', methods=['GET', 'POST'])
def api_status():
    """设备状态 API"""
    if request.method == 'POST':
        # 接收来自 ESP32 的状态更新
        data = request.get_json()
        device_id = data.get('device_id', config.DEVICE_ID)
        motion = data.get('motion', False)
        alarm = data.get('alarm', False)
        
        # 更新状态
        current_status['device_connected'] = True
        current_status['motion_detected'] = motion
        current_status['alarm_active'] = alarm
        current_status['last_update'] = time.time()
        
        # 记录到数据库
        status_str = 'motion_detected' if motion else 'idle'
        db.update_device_status(device_id, status_str)
        
        if motion:
            db.log(device_id, 'WARNING', '检测到人体运动')
            db.set_pending_action(device_id, True)
        
        return jsonify({'success': True, 'message': '状态更新成功'})
    
    # GET 请求返回当前状态
    return jsonify({
        'device_id': config.DEVICE_ID,
        'status': current_status,
        'timestamp': time.time()
    })

@app.route('/api/v1/command', methods=['POST'])
def api_command():
    """发送命令到设备"""
    data = request.get_json()
    command = data.get('command')
    
    if not command:
        return jsonify({'success': False, 'message': '缺少命令参数'}), 400
    
    # 处理命令
    if command == 'alarm_on':
        current_status['alarm_active'] = True
        db.log(config.DEVICE_ID, 'WARNING', '通过API触发警报')
    elif command == 'alarm_off':
        current_status['alarm_active'] = False
        db.log(config.DEVICE_ID, 'INFO', '通过API关闭警报')
    elif command == 'reset':
        current_status['motion_detected'] = False
        current_status['alarm_active'] = False
        db.log(config.DEVICE_ID, 'INFO', '通过API重置系统')
    else:
        return jsonify({'success': False, 'message': '未知命令'}), 400
    
    return jsonify({'success': True, 'message': f'命令执行成功: {command}'})

@app.route('/api/v1/logs', methods=['GET'])
def api_logs():
    """获取日志"""
    device_id = request.args.get('device_id')
    limit = int(request.args.get('limit', 20))
    logs = db.get_recent_logs(device_id, limit)
    
    # 格式化日志数据
    formatted_logs = []
    for log in logs:
        formatted_logs.append({
            'id': log[0],
            'device_id': log[1],
            'level': log[2],
            'message': log[3],
            'timestamp': log[4],
            'time_str': time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(log[4]))
        })
    
    return jsonify({'logs': formatted_logs})

# 错误处理
@app.errorhandler(404)
def page_not_found(e):
    return render_template('error.html', error='页面不存在'), 404

@app.errorhandler(500)
def internal_server_error(e):
    return render_template('error.html', error='服务器内部错误'), 500

if __name__ == '__main__':
    # 初始化设备状态
    db.update_device_status(config.DEVICE_ID, 'idle')
    db.log('SYSTEM', 'INFO', '智能实验室安全卫士启动')
    
    # 启动 Flask 应用
    app.run(host=config.HOST, port=config.PORT, debug=config.DEBUG)