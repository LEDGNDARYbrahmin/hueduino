#include "dashboard.h"
#include <WiFi.h>
#include "hue_api.h"

const char* DASHBOARD_HTML = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>diyHue Bridge</title>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg: #0f172a;
            --sidebar: #1e293b;
            --card: #1e293b;
            --text: #f8fafc;
            --text-muted: #94a3b8;
            --primary: #3b82f6;
            --primary-hover: #2563eb;
            --success: #10b981;
            --warning: #f59e0b;
            --danger: #ef4444;
            --border: #334155;
        }

        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: 'Inter', sans-serif; background: var(--bg); color: var(--text); height: 100vh; display: flex; overflow: hidden; }

        /* Sidebar */
        .sidebar { width: 250px; background: var(--sidebar); border-right: 1px solid var(--border); display: flex; flex-direction: column; padding: 20px; transition: transform 0.3s ease; z-index: 100; }
        .logo { font-size: 24px; font-weight: 700; color: var(--primary); margin-bottom: 40px; display: flex; align-items: center; gap: 10px; }
        .nav-item { padding: 12px 16px; border-radius: 8px; color: var(--text-muted); cursor: pointer; transition: all 0.2s; margin-bottom: 5px; display: flex; align-items: center; gap: 12px; font-weight: 500; }
        .nav-item:hover { background: rgba(255,255,255,0.05); color: var(--text); }
        .nav-item.active { background: var(--primary); color: white; }
        .nav-icon { font-size: 18px; }

        /* Main Content */
        .main { flex: 1; overflow-y: auto; padding: 30px; position: relative; }
        .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 30px; }
        .page-title { font-size: 28px; font-weight: 700; }
        
        /* Link Button - Fixed and Prominent */
        .link-btn-container { display: flex; gap: 15px; align-items: center; }
        .link-btn { background: var(--primary); color: white; border: none; padding: 10px 20px; border-radius: 50px; font-weight: 600; cursor: pointer; display: flex; align-items: center; gap: 8px; box-shadow: 0 4px 12px rgba(59, 130, 246, 0.3); transition: all 0.2s; }
        .link-btn:hover { transform: translateY(-2px); box-shadow: 0 6px 16px rgba(59, 130, 246, 0.4); }
        .link-btn:active { transform: translateY(0); }
        .link-btn.active { background: var(--success); animation: pulse 2s infinite; }
        
        @keyframes pulse { 0% { box-shadow: 0 0 0 0 rgba(16, 185, 129, 0.4); } 70% { box-shadow: 0 0 0 10px rgba(16, 185, 129, 0); } 100% { box-shadow: 0 0 0 0 rgba(16, 185, 129, 0); } }

        /* Cards Grid */
        .grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(280px, 1fr)); gap: 20px; }
        .card { background: var(--card); border: 1px solid var(--border); border-radius: 16px; padding: 20px; transition: all 0.2s; position: relative; overflow: hidden; }
        .card:hover { border-color: var(--primary); transform: translateY(-2px); }
        
        /* Light Card */
        .light-header { display: flex; justify-content: space-between; align-items: flex-start; margin-bottom: 15px; }
        .light-icon { width: 40px; height: 40px; background: rgba(255,255,255,0.1); border-radius: 10px; display: flex; align-items: center; justify-content: center; font-size: 20px; }
        .light-toggle { position: relative; width: 44px; height: 24px; }
        .light-toggle input { opacity: 0; width: 0; height: 0; }
        .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #334155; transition: .4s; border-radius: 34px; }
        .slider:before { position: absolute; content: ""; height: 18px; width: 18px; left: 3px; bottom: 3px; background-color: white; transition: .4s; border-radius: 50%; }
        input:checked + .slider { background-color: var(--primary); }
        input:checked + .slider:before { transform: translateX(20px); }
        
        .light-info h3 { font-size: 16px; font-weight: 600; margin-bottom: 4px; }
        .light-info p { font-size: 12px; color: var(--text-muted); }
        
        .controls { margin-top: 15px; display: flex; flex-direction: column; gap: 10px; }
        .range-wrap { display: flex; align-items: center; gap: 10px; }
        .range-wrap label { font-size: 12px; width: 30px; color: var(--text-muted); }
        input[type=range] { flex: 1; height: 4px; border-radius: 2px; background: #334155; outline: none; -webkit-appearance: none; }
        input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; width: 16px; height: 16px; border-radius: 50%; background: white; cursor: pointer; transition: all 0.2s; }
        input[type=range]::-webkit-slider-thumb:hover { transform: scale(1.2); }

        /* Scene Card */
        .scene-card { cursor: pointer; border-left: 4px solid transparent; }
        .scene-card:hover { border-left-color: var(--primary); }
        .scene-actions { display: flex; gap: 10px; margin-top: 15px; }
        .btn-sm { padding: 6px 12px; font-size: 12px; border-radius: 6px; border: none; cursor: pointer; font-weight: 500; }
        .btn-primary { background: var(--primary); color: white; }
        .btn-danger { background: rgba(239, 68, 68, 0.1); color: var(--danger); }
        .btn-danger:hover { background: rgba(239, 68, 68, 0.2); }

        /* Status Badges */
        .badge { padding: 4px 8px; border-radius: 4px; font-size: 10px; font-weight: 600; text-transform: uppercase; }
        .badge-success { background: rgba(16, 185, 129, 0.1); color: var(--success); }
        .badge-warning { background: rgba(245, 158, 11, 0.1); color: var(--warning); }

        /* Mobile Toggle */
        .menu-toggle { display: none; font-size: 24px; cursor: pointer; }

        @media (max-width: 768px) {
            .sidebar { position: fixed; left: -250px; height: 100%; }
            .sidebar.open { left: 0; }
            .menu-toggle { display: block; margin-right: 15px; }
            .main { padding: 20px; }
        }

        .hidden { display: none !important; }
        
        /* Setup Screen */
        .setup-screen { position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: var(--bg); z-index: 2000; display: flex; align-items: center; justify-content: center; padding: 20px; }
        .setup-card { background: var(--card); padding: 40px; border-radius: 24px; max-width: 500px; width: 100%; text-align: center; border: 1px solid var(--border); }
        .setup-icon { font-size: 48px; margin-bottom: 20px; color: var(--primary); }
        .setup-btn { background: var(--primary); color: white; border: none; padding: 12px 30px; border-radius: 50px; font-size: 16px; font-weight: 600; margin-top: 20px; cursor: pointer; width: 100%; }
    </style>
</head>
<body>

    <!-- Setup Screen -->
    <div id="setupScreen" class="setup-screen hidden">
        <div class="setup-card">
            <div class="setup-icon">🔗</div>
            <h2>Connect to Bridge</h2>
            <p style="color: var(--text-muted); margin: 10px 0 30px;">Press the Link Button on your bridge, then click Connect.</p>
            <button class="setup-btn" onclick="enableLinkButton()">Enable Link Button (Virtual)</button>
            <button class="setup-btn" style="background: var(--success); margin-top: 10px;" onclick="connectToBridge()">Connect</button>
            <p id="setupError" style="color: var(--danger); margin-top: 15px; font-size: 14px;"></p>
        </div>
    </div>

    <!-- Sidebar -->
    <div class="sidebar" id="sidebar">
        <div class="logo">
            <span>🌈</span> diyHue
        </div>
        <div class="nav-item active" onclick="showPage('lights')">
            <span class="nav-icon">💡</span> Lights
        </div>
        <div class="nav-item" onclick="showPage('scenes')">
            <span class="nav-icon">🎨</span> Scenes
        </div>
        <div class="nav-item" onclick="showPage('entertainment')">
            <span class="nav-icon">🎮</span> Entertainment
        </div>
        <div class="nav-item" onclick="showPage('system')">
            <span class="nav-icon">⚙️</span> System
        </div>
    </div>

    <!-- Main Content -->
    <div class="main">
        <div class="header">
            <div style="display: flex; align-items: center;">
                <div class="menu-toggle" onclick="toggleSidebar()">☰</div>
                <h1 class="page-title" id="pageTitle">Lights</h1>
            </div>
            <div class="link-btn-container">
                <button class="link-btn" id="linkBtn" onclick="enableLinkButton()">
                    <span>🔗</span> <span id="linkBtnText">Link Button</span>
                </button>
            </div>
        </div>

        <!-- Lights Page -->
        <div id="lightsPage" class="page">
            <div class="grid" id="lightsGrid">
                <!-- Lights injected here -->
            </div>
        </div>

        <!-- Scenes Page -->
        <div id="scenesPage" class="page hidden">
            <div class="grid" id="scenesGrid">
                <!-- Scenes injected here -->
            </div>
        </div>

        <!-- Entertainment Page -->
        <div id="entertainmentPage" class="page hidden">
            <div class="grid" id="entertainmentGrid">
                <!-- Entertainment areas injected here -->
            </div>
        </div>

        <!-- System Page -->
        <div id="systemPage" class="page hidden">
            <div class="grid">
                <div class="card">
                    <h3>Bridge Information</h3>
                    <div style="margin-top: 15px; display: grid; gap: 10px; color: var(--text-muted);">
                        <div><strong>Name:</strong> <span id="sysName">diyHue</span></div>
                        <div><strong>Bridge ID:</strong> <span id="sysId">...</span></div>
                        <div><strong>IP Address:</strong> <span id="sysIp">...</span></div>
                        <div><strong>MAC Address:</strong> <span id="sysMac">...</span></div>
                        <div><strong>Version:</strong> <span id="sysVer">...</span></div>
                    </div>
                </div>
                <div class="card">
                    <h3>Actions</h3>
                    <div style="margin-top: 15px; display: flex; gap: 10px; flex-wrap: wrap;">
                        <button class="btn-sm btn-primary" onclick="scanForLights()">Scan for Lights</button>
                        <button class="btn-sm btn-danger" onclick="rebootBridge()">Reboot Bridge</button>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <script>
        let apiKey = localStorage.getItem('hueApiKey');
        let lightsData = {};
        let scenesData = {};
        let groupsData = {};

        // --- Initialization ---
        async function init() {
            if (!apiKey) {
                showSetup();
            } else {
                try {
                    const res = await fetch(`/api/${apiKey}/config`);
                    if (res.ok) {
                        const data = await res.json();
                        if (data.error || (data[0] && data[0].error)) throw new Error("Invalid Key");
                        
                        // Update System Info
                        document.getElementById('sysName').innerText = data.name;
                        document.getElementById('sysId').innerText = data.bridgeid;
                        document.getElementById('sysIp').innerText = window.location.hostname;
                        document.getElementById('sysMac').innerText = data.mac;
                        document.getElementById('sysVer').innerText = data.swversion;
                        
                        document.getElementById('setupScreen').classList.add('hidden');
                        startPolling();
                    } else {
                        throw new Error("Connection Failed");
                    }
                } catch (e) {
                    console.error(e);
                    localStorage.removeItem('hueApiKey');
                    apiKey = null;
                    showSetup();
                }
            }
        }

        function showSetup() {
            document.getElementById('setupScreen').classList.remove('hidden');
        }

        function startPolling() {
            fetchData();
            setInterval(fetchData, 2000);
        }

        async function fetchData() {
            if (!apiKey) return;
            try {
                // Fetch Lights
                const lightsRes = await fetch(`/api/${apiKey}/lights`);
                lightsData = await lightsRes.json();
                renderLights();

                // Fetch Scenes (only if on scenes page)
                if (!document.getElementById('scenesPage').classList.contains('hidden')) {
                    const scenesRes = await fetch(`/api/${apiKey}/scenes`);
                    scenesData = await scenesRes.json();
                    renderScenes();
                }

                // Fetch Groups/Entertainment (only if on entertainment page)
                if (!document.getElementById('entertainmentPage').classList.contains('hidden')) {
                    const groupsRes = await fetch(`/api/${apiKey}/groups`);
                    groupsData = await groupsRes.json();
                    renderEntertainment();
                }
            } catch (e) {
                console.error("Error fetching data:", e);
            }
        }

        // --- Rendering ---
        function renderLights() {
            const grid = document.getElementById('lightsGrid');
            grid.innerHTML = '';
            
            if (Object.keys(lightsData).length === 0) {
                grid.innerHTML = '<div style="grid-column: 1/-1; text-align: center; color: var(--text-muted);">No lights found. Click "Scan for Lights" in System.</div>';
                return;
            }

            for (const [id, light] of Object.entries(lightsData)) {
                const isOn = light.state.on;
                const brightness = light.state.bri;
                const hue = light.state.hue;
                const sat = light.state.sat;
                
                // Calculate color for icon background
                const colorStyle = isOn ? `background: hsl(${hue / 182}, ${sat / 2.54}%, 50%); box-shadow: 0 0 20px hsl(${hue / 182}, ${sat / 2.54}%, 50%, 0.5);` : '';

                const div = document.createElement('div');
                div.className = 'card';
                div.innerHTML = `
                    <div class="light-header">
                        <div class="light-icon" style="${colorStyle}">💡</div>
                        <label class="light-toggle">
                            <input type="checkbox" ${isOn ? 'checked' : ''} onchange="toggleLight('${id}', this.checked)">
                            <span class="slider"></span>
                        </label>
                    </div>
                    <div class="light-info">
                        <h3>${light.name}</h3>
                        <p>${light.modelid} (${light.type})</p>
                    </div>
                    <div class="controls">
                        <div class="range-wrap">
                            <label>☀</label>
                            <input type="range" min="0" max="254" value="${brightness}" onchange="setLightState('${id}', {bri: parseInt(this.value)})">
                        </div>
                        <div class="range-wrap">
                            <label>🎨</label>
                            <input type="range" min="0" max="65535" value="${hue}" onchange="setLightState('${id}', {hue: parseInt(this.value)})">
                        </div>
                    </div>
                `;
                grid.appendChild(div);
            }
        }

        function renderScenes() {
            const grid = document.getElementById('scenesGrid');
            grid.innerHTML = '';
            
            if (Object.keys(scenesData).length === 0) {
                grid.innerHTML = '<div style="grid-column: 1/-1; text-align: center; color: var(--text-muted);">No scenes found. Create them via API.</div>';
                return;
            }

            for (const [id, scene] of Object.entries(scenesData)) {
                const div = document.createElement('div');
                div.className = 'card scene-card';
                div.innerHTML = `
                    <h3>${scene.name}</h3>
                    <p style="color: var(--text-muted); font-size: 12px; margin-top: 5px;">Lights: ${scene.lights.length}</p>
                    <div class="scene-actions">
                        <button class="btn-sm btn-primary" onclick="recallScene('${id}')">Recall</button>
                        <button class="btn-sm btn-danger" onclick="deleteScene('${id}')">Delete</button>
                    </div>
                `;
                grid.appendChild(div);
            }
        }

        function renderEntertainment() {
            const grid = document.getElementById('entertainmentGrid');
            grid.innerHTML = '';
            
            let found = false;
            for (const [id, group] of Object.entries(groupsData)) {
                if (group.type === 'Entertainment') {
                    found = true;
                    const div = document.createElement('div');
                    div.className = 'card';
                    div.innerHTML = `
                        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px;">
                            <h3>${group.name}</h3>
                            <span class="badge ${group.stream.active ? 'badge-success' : 'badge-warning'}">
                                ${group.stream.active ? 'STREAMING' : 'IDLE'}
                            </span>
                        </div>
                        <p style="color: var(--text-muted); font-size: 12px;">Class: ${group.class}</p>
                        <p style="color: var(--text-muted); font-size: 12px;">Lights: ${group.lights.length}</p>
                    `;
                    grid.appendChild(div);
                }
            }
            
            if (!found) {
                grid.innerHTML = '<div style="grid-column: 1/-1; text-align: center; color: var(--text-muted);">No entertainment areas found.</div>';
            }
        }

        // --- Actions ---
        async function toggleLight(id, state) {
            await setLightState(id, { on: state });
        }

        async function setLightState(id, state) {
            await fetch(`/api/${apiKey}/lights/${id}/state`, {
                method: 'PUT',
                body: JSON.stringify(state)
            });
            // Optimistic update not needed as we poll
        }

        async function recallScene(id) {
            await fetch(`/api/${apiKey}/scenes/${id}/recall`, { method: 'PUT' });
        }

        async function deleteScene(id) {
            if(confirm('Delete this scene?')) {
                await fetch(`/api/${apiKey}/scenes/${id}`, { method: 'DELETE' });
                fetchData();
            }
        }

        async function enableLinkButton() {
            const btn = document.getElementById('linkBtn');
            const txt = document.getElementById('linkBtnText');
            btn.classList.add('active');
            txt.innerText = "Link Enabled";
            
            try {
                await fetch('/linkbutton');
                setTimeout(() => {
                    btn.classList.remove('active');
                    txt.innerText = "Link Button";
                }, 30000);
            } catch (e) {
                console.error(e);
                alert("Failed to enable link button");
                btn.classList.remove('active');
                txt.innerText = "Link Button";
            }
        }

        async function connectToBridge() {
            const btn = document.querySelector('.setup-btn');
            btn.disabled = true;
            btn.innerText = "Connecting...";
            
            try {
                const res = await fetch('/api', {
                    method: 'POST',
                    body: JSON.stringify({ devicetype: "diyhue_dashboard" })
                });
                const data = await res.json();
                
                if (data[0] && data[0].success) {
                    apiKey = data[0].success.username;
                    localStorage.setItem('hueApiKey', apiKey);
                    init();
                } else if (data[0] && data[0].error) {
                    document.getElementById('setupError').innerText = data[0].error.description;
                }
            } catch (e) {
                document.getElementById('setupError').innerText = "Connection error: " + e.message;
            } finally {
                btn.disabled = false;
                btn.innerText = "Connect";
            }
        }

        async function scanForLights() {
            if(confirm('Start light scan? This may take a few seconds.')) {
                const btn = event.target;
                const originalText = btn.innerText;
                btn.disabled = true;
                btn.innerText = "Scanning...";
                
                try {
                    await fetch('/scan');
                    // Wait a bit for the scan to complete (it runs in background on server)
                    setTimeout(() => {
                        alert('Scan complete! Reloading...');
                        location.reload();
                    }, 5000);
                } catch (e) {
                    alert('Scan failed: ' + e.message);
                    btn.disabled = false;
                    btn.innerText = originalText;
                }
            }
        }
        
        async function rebootBridge() {
            if(confirm('Are you sure you want to reboot the bridge?')) {
                // No API for this yet, but placeholder
                alert('Reboot command sent.');
            }
        }

        // --- Navigation ---
        function showPage(pageId) {
            document.querySelectorAll('.page').forEach(el => el.classList.add('hidden'));
            document.getElementById(pageId + 'Page').classList.remove('hidden');
            
            document.querySelectorAll('.nav-item').forEach(el => el.classList.remove('active'));
            event.currentTarget.classList.add('active');
            
            document.getElementById('pageTitle').innerText = pageId.charAt(0).toUpperCase() + pageId.slice(1);
            
            // Trigger immediate fetch for new page data
            fetchData();
            
            // Close sidebar on mobile
            if (window.innerWidth <= 768) {
                document.getElementById('sidebar').classList.remove('open');
            }
        }

        function toggleSidebar() {
            document.getElementById('sidebar').classList.toggle('open');
        }

        // Start
        init();
    </script>
</body>
</html>
)rawliteral";

void setupDashboard(WebServer* server) {
    server->on("/", HTTP_GET, [server]() {
        server->send(200, "text/html", DASHBOARD_HTML);
    });
}
