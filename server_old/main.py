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
    'relay_active': False,  # 继电器状态（控制LED灯）
    'last_update': time.time(),
    'last_motion_time': time.time(),
    'countdown_active': False,
    'countdown_start': 0,
    'countdown_duration': 3 * 60,  # 3分钟倒计时
    'pending_command': None
}


@app.route('/')
def index():
    """主页面"""
    devices = db.get_all_devices()
    logs = db.get_recent_logs(limit=10)

    # 计算倒计时剩余时间
    remaining_time = 0
    if current_status['countdown_active']:
        elapsed = time.time() - current_status['countdown_start']
        remaining_time = max(0, current_status['countdown_duration'] - elapsed)
        # 倒计时结束，执行断电操作
        if remaining_time <= 0:
            current_status['countdown_active'] = False
            current_status['relay_active'] = False
            current_status['pending_command'] = 'relay_off'
            db.log(config.DEVICE_ID, 'INFO', '倒计时结束，自动断电')

    return render_template('index.html', status=current_status, devices=devices, logs=logs,
                           remaining_time=remaining_time)


@app.route('/status')
def status():
    """状态页面"""
    device = db.get_device_status(config.DEVICE_ID)
    logs = db.get_recent_logs(config.DEVICE_ID, limit=20)

    # 计算倒计时剩余时间
    remaining_time = 0
    if current_status['countdown_active']:
        elapsed = time.time() - current_status['countdown_start']
        remaining_time = max(0, current_status['countdown_duration'] - elapsed)

    return render_template('status.html', device=device, logs=logs, current_status=current_status, time=time,
                           remaining_time=remaining_time)


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
            if decision == 'keep_power':
                # 保持通电，重置倒计时
                current_status['countdown_active'] = False
                current_status['relay_active'] = True
                current_status['pending_command'] = 'relay_on'
                db.log(config.DEVICE_ID, 'INFO', '用户选择保持通电')
            elif decision == 'power_off':
                # 立即断电
                current_status['countdown_active'] = False
                current_status['relay_active'] = False
                current_status['pending_command'] = 'relay_off'
                db.log(config.DEVICE_ID, 'INFO', '用户选择立即断电')
            elif decision == 'reset':
                # 重置系统
                current_status['motion_detected'] = False
                current_status['relay_active'] = False
                current_status['countdown_active'] = False
                current_status['pending_command'] = None
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
        relay = data.get('relay', False)

        # 更新状态
        current_status['device_connected'] = True
        current_status['last_update'] = time.time()

        # 处理运动检测
        if motion != current_status['motion_detected']:
            current_status['motion_detected'] = motion
            if motion:
                current_status['last_motion_time'] = time.time()
                current_status['relay_active'] = True
                current_status['countdown_active'] = False
                current_status['pending_command'] = 'relay_on'
                db.log(device_id, 'INFO', '检测到人体运动，保持通电')
            else:
                # 检测不到人，开始倒计时
                current_status['countdown_active'] = True
                current_status['countdown_start'] = time.time()
                db.log(device_id, 'INFO', '未检测到人体运动，开始断电倒计时')

        # 记录到数据库
        status_str = 'motion_detected' if motion else 'idle'
        db.update_device_status(device_id, status_str)

        if motion:
            db.set_pending_action(device_id, True)

        return jsonify({'success': True, 'message': '状态更新成功'})

    # GET 请求返回当前状态
    return jsonify({
        'device_id': config.DEVICE_ID,
        'status': current_status,
        'timestamp': time.time()
    })


@app.route('/api/v1/command', methods=['GET', 'POST'])
def api_command():
    """发送命令到设备"""
    if request.method == 'POST':
        # 接收来自网页的命令
        data = request.get_json()
        command = data.get('command')

        if not command:
            return jsonify({'success': False, 'message': '缺少命令参数'}), 400

        # 处理命令
        if command == 'keep_power':
            current_status['countdown_active'] = False
            current_status['relay_active'] = True
            current_status['pending_command'] = 'relay_on'
            db.log(config.DEVICE_ID, 'INFO', '通过API保持通电')
        elif command == 'power_off':
            current_status['countdown_active'] = False
            current_status['relay_active'] = False
            current_status['pending_command'] = 'relay_off'
            db.log(config.DEVICE_ID, 'INFO', '通过API立即断电')
        elif command == 'reset':
            current_status['motion_detected'] = False
            current_status['relay_active'] = False
            current_status['countdown_active'] = False
            current_status['pending_command'] = None
            db.log(config.DEVICE_ID, 'INFO', '通过API重置系统')
        else:
            return jsonify({'success': False, 'message': '未知命令'}), 400

        return jsonify({'success': True, 'message': f'命令执行成功: {command}'})

    # GET 请求：ESP32 获取待执行命令
    command = current_status['pending_command']
    # 清除待执行命令
    if command:
        current_status['pending_command'] = None

    return jsonify({
        'device_id': config.DEVICE_ID,
        'command': command,
        'timestamp': time.time()
    })


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
    db.log('SYSTEM', 'INFO', '智能断电装置启动')

    # 启动 Flask 应用
    app.run(host=config.HOST, port=config.PORT, debug=config.DEBUG)
