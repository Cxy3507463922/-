from flask import Flask, render_template
import os

app = Flask(__name__)

def test_templates():
    """测试所有模板文件是否能正常渲染"""
    templates_dir = 'templates'
    template_files = [f for f in os.listdir(templates_dir) if f.endswith('.html')]
    
    print(f"开始测试模板文件，共 {len(template_files)} 个文件")
    
    with app.app_context():
        for template_file in template_files:
            try:
                # 添加必要的上下文变量
                context = {
                    'status': {'device_connected': False, 'motion_detected': False, 'alarm_active': False, 'last_update': 1234567890},
                    'devices': [('esp32_smart_guardian', 'idle', 1234567890, 0, None, 1234567890)],
                    'logs': [(1, 'esp32_smart_guardian', 'INFO', '测试日志', 1234567890)],
                    'device': ('esp32_smart_guardian', 'idle', 1234567890, 0, None, 1234567890),
                    'decisions': [(1, 'esp32_smart_guardian', 'alarm_on', 1234567890, '测试决策')],
                    'current_status': {'device_connected': False, 'motion_detected': False, 'alarm_active': False, 'last_update': 1234567890},
                    'time': __import__('time')
                }
                
                # 渲染模板
                rendered = render_template(template_file, **context)
                print(f"✓ {template_file}: 渲染成功")
            except Exception as e:
                print(f"✗ {template_file}: 渲染失败 - {str(e)}")
                import traceback
                traceback.print_exc()

if __name__ == '__main__':
    test_templates()