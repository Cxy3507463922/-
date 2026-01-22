const path = require('path');

module.exports = {
    PORT: 5000,
    DATABASE_PATH: path.join(__dirname, 'data', 'guardian.db'),
    DEVICE_ID: 'esp32_smart_guardian',
    DEVICE_TIMEOUT: 30,
    ALARM_DURATION: 5,
    SENSITIVITY: 'medium',
    REFRESH_INTERVAL: 60 // seconds
};
