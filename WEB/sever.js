const express = require('express');
const app = express();
const http = require('http').createServer(app);
const io = require('socket.io')(http); 
const mqtt = require('mqtt');
const schedule = require('node-schedule');
const fs = require('fs');
const path = require('path');
const cors = require('cors');

// --- CẤU HÌNH ---
const PORT = 3000;

// THÔNG TIN MQTT CỦA BẠN
const MQTT_BROKER_HOST = 'd6a721cd5a254421a2b876af2e91c31d.s1.eu.hivemq.cloud';
const MQTT_USERNAME = 'Chiendlb';     
const MQTT_PASSWORD = 'Chiendlb456'; 

// CẤU HÌNH TOPIC MỚI THEO YÊU CẦU
// Topic điều khiển sẽ là: swDevice/MAC
const TOPIC_CONTROL_PREFIX = 'swDevice'; 
const TOPIC_DISCOVERY = 'swDevice/+'; 
const DATA_FILE = 'data.json';

// --- MIDDLEWARE ---
app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, 'view'))); 

let activeJobs = {}; 

// --- 1. QUẢN LÝ DỮ LIỆU ---
function loadData() {
    if (!fs.existsSync(DATA_FILE)) {
        const defaultData = { devices: [] };
        fs.writeFileSync(DATA_FILE, JSON.stringify(defaultData));
        return defaultData;
    }
    return JSON.parse(fs.readFileSync(DATA_FILE));
}

function saveData(data) {
    fs.writeFileSync(DATA_FILE, JSON.stringify(data, null, 2));
}

let db = loadData();

// --- 2. KẾT NỐI MQTT ---
const client = mqtt.connect(`mqtts://${MQTT_BROKER_HOST}`, {
    protocol: 'mqtts',
    port: 8883,
    username: MQTT_USERNAME,
    password: MQTT_PASSWORD,
    clientId: 'NodeServer_' + Math.random().toString(16).substr(2, 8) 
});

client.on('connect', () => {
    console.log('✅ Đã kết nối tới HiveMQ (MQTTs 8883)');
    // Đăng ký nhận tin nhắn từ các thiết bị (Discovery & Status report)
    client.subscribe(`${TOPIC_CONTROL_PREFIX}/+`);
});

client.on('message', (topic, message) => {
    // Topic dạng: swDevice/AA:BB:CC...
    if (topic.startsWith(`${TOPIC_CONTROL_PREFIX}/`)) {
        const macAddress = topic.split('/')[1];
        
        // 1. LOGIC DISCOVERY: Nếu thiết bị chưa có trong DB -> Báo Web hiện Popup
        const isExist = db.devices.some(d => d.id === macAddress);
        if (!isExist) {
            // Chỉ coi là discovery nếu message không phải JSON điều khiển (ví dụ message text 'Hello')
            // Hoặc đơn giản cứ báo lên Web, Web tự check duplicate
            console.log(`🔎 Nhận tin từ thiết bị lạ: ${macAddress}`);
            io.emit('new_device_found', { mac: macAddress });
        }

        // 2. LOGIC CẬP NHẬT TRẠNG THÁI (Nếu thiết bị gửi báo cáo trạng thái về)
        try {
            const payload = JSON.parse(message.toString());
            // Payload ví dụ: { "ch1_status": 1, "ch2_status": 0, "ch3_status": 1 }
            
            const device = db.devices.find(d => d.id === macAddress);
            if (device) {
                let hasChange = false;
                
                // Duyệt qua các key trong JSON để update status
                // channelIndex = 0 (Kênh 1), 1 (Kênh 2)...
                device.channels.forEach((ch, index) => {
                    const key = `ch${index + 1}_status`; // ch1_status, ch2_status...
                    if (payload.hasOwnProperty(key)) {
                        const newStatus = parseInt(payload[key]);
                        if (ch.status !== newStatus) {
                            ch.status = newStatus;
                            hasChange = true;
                            // Báo Web cập nhật nút bấm
                            io.emit('update_switch', { deviceId: macAddress, channelIndex: index, status: newStatus });
                        }
                    }
                });

                if (hasChange) {
                    saveData(db);
                    console.log(`📡 Cập nhật trạng thái từ thiết bị ${macAddress}`);
                }
            }
        } catch (e) {
            // Không phải JSON -> Bỏ qua (có thể là tin nhắn Discovery text)
        }
    }
});

// --- 3. SOCKET.IO ---
io.on('connection', (socket) => {
    console.log('👤 Web UI đã kết nối');
    socket.emit('init_data', db.devices);
});

// --- 4. HỆ THỐNG HẸN GIỜ ---
function restoreTimers() {
    console.log("🔄 Đang khôi phục các lịch hẹn giờ...");
    db.devices.forEach(device => {
        device.channels.forEach((ch, chIndex) => {
            if (ch.timeOn) scheduleTask(device.id, chIndex, 1, ch.timeOn);
            if (ch.timeOff) scheduleTask(device.id, chIndex, 0, ch.timeOff);
        });
    });
}

function scheduleTask(deviceId, channelIndex, action, time) {
    const jobKey = `${deviceId}_ch${channelIndex}_${action}`; 
    const [hour, minute] = time.split(':');

    if (activeJobs[jobKey]) activeJobs[jobKey].cancel();
    if (!time) return;

    const rule = new schedule.RecurrenceRule();
    rule.hour = parseInt(hour);
    rule.minute = parseInt(minute);
    rule.second = 0; 

    const job = schedule.scheduleJob(rule, function() {
        console.log(`⏰ ĐẾN GIỜ HẸN: ${deviceId} Kênh ${channelIndex} -> ${action}`);
        
        // --- LOGIC GỬI JSON KHI ĐẾN GIỜ ---
        sendControlCommand(deviceId, channelIndex, action);
    });

    activeJobs[jobKey] = job;
}

// --- HÀM GỬI LỆNH ĐIỀU KHIỂN CHUNG (Dùng cho cả API và Timer) ---
function sendControlCommand(deviceId, channelIndex, status) {
    const device = db.devices.find(d => d.id === deviceId);
    if (!device) return;

    // 1. Cập nhật trạng thái vào DB trước (để lấy trạng thái hiện tại của các kênh khác)
    if (device.channels[channelIndex]) {
        device.channels[channelIndex].status = status;
        saveData(db);
        
        // Cập nhật giao diện Web ngay lập tức (cho mượt)
        io.emit('update_switch', { deviceId, channelIndex, status });
    }

    // 2. Tạo JSON Payload chứa trạng thái TOÀN BỘ các kênh
    // Ví dụ: { "ch1_status": 1, "ch2_status": 0, "ch3_status": 1 }
    let payload = {};
    device.channels.forEach((ch, index) => {
        // channelIndex thực tế (0,1,2) -> Key JSON (ch1_status, ch2_status...)
        payload[`ch${index + 1}_status`] = ch.status;
    });

    const topic = `${TOPIC_CONTROL_PREFIX}/${deviceId}`;
    
    // Gửi JSON lên MQTT
    client.publish(topic, JSON.stringify(payload), { retain: true, qos: 1 });
    console.log(`📤 Gửi lệnh tới ${topic}:`, JSON.stringify(payload));
}

// --- 5. API ROUTES ---

app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'view', 'HOME.html'));
});

app.post('/api/devices', (req, res) => {
    const newDevice = req.body;
    if (!db.devices.find(d => d.id === newDevice.id)) {
        db.devices.push(newDevice);
        saveData(db);
        io.emit('init_data', db.devices);
        res.json({ success: true, message: 'Thêm thiết bị thành công' });
    } else {
        res.status(400).json({ success: false, message: 'Thiết bị đã tồn tại' });
    }
});

app.delete('/api/devices/:id', (req, res) => {
    const id = req.params.id;
    const device = db.devices.find(d => d.id === id);
    if (device) {
        device.channels.forEach((_, index) => {
            if (activeJobs[`${id}_ch${index}_0`]) activeJobs[`${id}_ch${index}_0`].cancel();
            if (activeJobs[`${id}_ch${index}_1`]) activeJobs[`${id}_ch${index}_1`].cancel();
        });
    }

    db.devices = db.devices.filter(d => d.id !== id);
    saveData(db);
    io.emit('init_data', db.devices); 
    res.json({ success: true, message: 'Đã xóa thiết bị' });
});

// API: Điều khiển Bật/Tắt (Được gọi từ Frontend)
app.put('/api/switch/control', (req, res) => {
    const { deviceId, channelIndex, status } = req.body;
    
    // Gọi hàm xử lý chung để gửi JSON
    sendControlCommand(deviceId, channelIndex, status);

    res.json({ success: true, message: 'Đã gửi lệnh JSON' });
});

app.post('/api/timer/set', (req, res) => {
    const { deviceId, channelIndex, timeOn, timeOff } = req.body;
    
    const device = db.devices.find(d => d.id === deviceId);
    if (device && device.channels[channelIndex]) {
        device.channels[channelIndex].timeOn = timeOn;
        device.channels[channelIndex].timeOff = timeOff;
        saveData(db);

        scheduleTask(deviceId, channelIndex, 1, timeOn); 
        scheduleTask(deviceId, channelIndex, 0, timeOff); 
        io.emit('init_data', db.devices);
        res.json({ success: true, message: 'Đã lưu hẹn giờ' });
    } else {
        res.status(404).json({ success: false, message: 'Không tìm thấy thiết bị' });
    }
});

restoreTimers();

http.listen(PORT, () => {
    console.log(`🚀 Server đang chạy tại http://localhost:${PORT}`);
    console.log(`📡 MQTT Broker: ${MQTT_BROKER_HOST} (MQTTs 8883)`);
});