#ifndef WEB_PAGES_H
#define WEB_PAGES_H

#include <Arduino.h>

const char HTML_INDEX[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Awning Controller</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 20px;
            background-color: #f0f0f0;
        }
        .container {
            max-width: 600px;
            margin: 0 auto;
            background: white;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 {
            color: #333;
            text-align: center;
        }
        .status {
            background: #e8f4fd;
            padding: 15px;
            border-radius: 5px;
            margin-bottom: 20px;
        }
        .controls {
            display: grid;
            gap: 10px;
            margin-bottom: 20px;
        }
        button {
            padding: 12px 20px;
            font-size: 16px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
        }
        .btn-open {
            background: #4CAF50;
            color: white;
        }
        .btn-close {
            background: #2196F3;
            color: white;
        }
        .btn-stop {
            background: #f44336;
            color: white;
        }
        .btn-config {
            background: #FF9800;
            color: white;
        }
        .position-control {
            display: flex;
            align-items: center;
            gap: 10px;
            margin: 10px 0;
        }
        .position-slider {
            flex: 1;
        }
        input[type="range"] {
            width: 100%;
        }
        input[type="number"] {
            width: 80px;
            padding: 5px;
        }
        .config-section {
            margin-top: 20px;
            padding-top: 20px;
            border-top: 1px solid #ddd;
        }
        .form-group {
            margin: 10px 0;
        }
        label {
            display: inline-block;
            width: 150px;
            font-weight: bold;
        }
        .wind-info {
            background: #fff3cd;
            padding: 10px;
            border-radius: 5px;
            margin: 10px 0;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Awning Controller</h1>
        
        <div class="status" id="status">
            <div><strong>Position:</strong> <span id="position">--%</span></div>
            <div><strong>Motor:</strong> <span id="motor">--</span></div>
            <div><strong>Wind Pulses:</strong> <span id="windPulses">-- /min</span></div>
            <div><strong>Target:</strong> <span id="target">--%</span></div>
        </div>
        
        <div class="controls">
            <button class="btn-open" onclick="sendCommand('open')">OPEN</button>
            <button class="btn-close" onclick="sendCommand('close')">CLOSE</button>
            <button class="btn-stop" onclick="sendCommand('stop')">STOP</button>
        </div>
        
        <div class="position-control">
            <label>Position:</label>
            <input type="range" id="positionSlider" min="0" max="100" value="50" class="position-slider">
            <input type="number" id="positionValue" min="0" max="100" value="50">%
            <button onclick="setPosition()">Set</button>
        </div>
        
        <div class="config-section">
            <h3>Configuration</h3>
            
            <div class="form-group">
                <label>Travel Time:</label>
                <span id="currentTravelTime">Loading...</span> ms
            </div>
            
            <div class="calibration-section">
                <h4>Calibration</h4>
                <p>1. Ensure awning is at 0% position</p>
                <p>2. Click Start to begin extension</p>
                <p>3. Click Stop when fully extended</p>
                <button class="btn-config" id="calibrateBtn" onclick="toggleCalibration()">Start Calibration</button>
                <div id="calibrationStatus" style="display: none; margin-top: 10px; padding: 10px; background: #fff3cd; border: 1px solid #ffeaa7; border-radius: 4px;">
                    <strong>Calibration in progress...</strong><br>
                    Click "Stop Calibration" when awning reaches full extension.
                </div>
            </div>
            
            <div class="wind-info">
                <strong>Wind Safety:</strong> Awning will automatically close if pulse count exceeds threshold
            </div>
            
            <div class="form-group">
                <label>Wind Threshold:</label>
                <input type="number" id="windThreshold" min="0" max="1000" value="100"> pulses/min
                <button class="btn-config" onclick="setWindThreshold()">Set</button>
            </div>
            
            <div class="form-group" style="text-align: center; margin-top: 20px;">
                <button class="btn-config" onclick="window.location.href='/system-config'">System Configuration</button>
            </div>
        </div>
    </div>

    <script>
        // Sync slider and number input
        document.getElementById('positionSlider').oninput = function() {
            document.getElementById('positionValue').value = this.value;
        };
        document.getElementById('positionValue').oninput = function() {
            document.getElementById('positionSlider').value = this.value;
        };
        
        function sendCommand(action) {
            fetch('/control', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: 'action=' + action
            }).then(response => {
                if (!response.ok) throw new Error('Command failed');
                updateStatus();
            }).catch(err => alert('Error: ' + err.message));
        }
        
        function setPosition() {
            const position = document.getElementById('positionValue').value;
            fetch('/control', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: 'action=position&value=' + position
            }).then(response => {
                if (!response.ok) throw new Error('Set position failed');
                updateStatus();
            }).catch(err => alert('Error: ' + err.message));
        }
        
        function toggleCalibration() {
            fetch('/calibrate', {
                method: 'POST'
            }).then(response => response.text()).then(message => {
                if (message.includes('started')) {
                    document.getElementById('calibrateBtn').textContent = 'Stop Calibration';
                    document.getElementById('calibrationStatus').style.display = 'block';
                } else if (message.includes('completed')) {
                    document.getElementById('calibrateBtn').textContent = 'Start Calibration';
                    document.getElementById('calibrationStatus').style.display = 'none';
                    alert('Calibration completed successfully!');
                    updateStatus();
                } else {
                    alert('Error: ' + message);
                }
            }).catch(err => alert('Error: ' + err.message));
        }
        
        function setWindThreshold() {
            const threshold = document.getElementById('windThreshold').value;
            fetch('/wind-config', {
                method: 'POST',
                headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                body: 'threshold=' + threshold
            }).then(response => {
                if (!response.ok) throw new Error('Wind threshold update failed');
                alert('Wind threshold updated');
                updateStatus();
            }).catch(err => alert('Error: ' + err.message));
        }
        
        
        function updateStatus() {
            fetch('/status')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('position').textContent = data.position.toFixed(1) + '%';
                    document.getElementById('target').textContent = data.target.toFixed(1) + '%';
                    document.getElementById('motor').textContent = data.motor;
                    document.getElementById('windPulses').textContent = data.windPulses + ' /min';
                    
                    // Update travel time display and calibration state
                    document.getElementById('currentTravelTime').textContent = data.travelTime;
                    document.getElementById('windThreshold').value = data.windThreshold;
                    
                    // Update calibration UI state
                    if (data.calibrating) {
                        document.getElementById('calibrateBtn').textContent = 'Stop Calibration';
                        document.getElementById('calibrationStatus').style.display = 'block';
                    } else {
                        document.getElementById('calibrateBtn').textContent = 'Start Calibration';
                        document.getElementById('calibrationStatus').style.display = 'none';
                    }
                })
                .catch(err => console.error('Status update failed:', err));
        }
        
        // Auto-refresh status every 2 seconds
        setInterval(updateStatus, 2000);
        
        // Initial status load
        updateStatus();
    </script>
</body>
</html>
)rawliteral";

#endif // WEB_PAGES_H