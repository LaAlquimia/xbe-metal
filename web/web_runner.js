/**
 * xbe-metal: WebRunner & WebGPU Bridge
 * Handles browser lifecycle, WebGPU canvas initialization, XBE drag & drop parsing, and UI telemetry.
 */

(function () {
    console.log("[xbe-metal] Initializing WebRunner runtime client...");

    const state = {
        canvas: document.getElementById('canvas'),
        gl: null,
        gpuDevice: null,
        isWebGPU: false,
        fpsCounter: document.getElementById('fps-val'),
        statusBadge: document.getElementById('status-val'),
        titleBadge: document.getElementById('title-val'),
        fileInfo: document.getElementById('file-info'),
        terminal: document.getElementById('terminal'),
        frameCount: 0,
        lastFpsUpdate: performance.now(),
        rotation: 0.0,
        isRunning: true,
        loadedXbe: null
    };

    function log(msg, type = 'info') {
        if (!state.terminal) return;
        const entry = document.createElement('div');
        entry.className = `log-entry ${type}`;
        const time = new Date().toISOString().substring(11, 19);
        entry.textContent = `[${time}] ${msg}`;
        state.terminal.appendChild(entry);
        state.terminal.scrollTop = state.terminal.scrollHeight;
    }

    // ========================================================================
    // Pure JS XBE Header Parser (Client-side fast inspection)
    // ========================================================================
    function parseXbeHeader(buffer) {
        if (buffer.byteLength < 4096) {
            throw new Error("File size too small for Xbox Executable.");
        }

        const view = new DataView(buffer);
        const magic = view.getUint32(0, true);
        if (magic !== 0x48454258) { // "XBEH" in little-endian
            throw new Error("Invalid XBE Magic identifier. Expected 'XBEH'.");
        }

        const baseAddress = view.getUint32(260, true);
        const sizeOfHeaders = view.getUint32(264, true);
        const sizeOfImage = view.getUint32(268, true);
        const certAddress = view.getUint32(280, true);
        const numberOfSections = view.getUint32(284, true);
        const sectionHeadersAddress = view.getUint32(288, true);
        const entryPoint = view.getUint32(296, true);

        // Retail entry point key: 0xA8FC57AB
        const decodedEntryPoint = entryPoint ^ 0xA8FC57AB;

        let titleName = "Untitled Xbox Game";
        let titleId = 0;

        if (certAddress >= baseAddress) {
            const certOffset = certAddress - baseAddress;
            if (certOffset + 100 < buffer.byteLength) {
                titleId = view.getUint32(certOffset + 8, true);
                let rawName = "";
                for (let i = 0; i < 40; ++i) {
                    const charCode = view.getUint16(certOffset + 12 + (i * 2), true);
                    if (charCode === 0) break;
                    rawName += String.fromCharCode(charCode);
                }
                if (rawName.trim().length > 0) {
                    titleName = rawName.trim();
                }
            }
        }

        return {
            magic: "XBEH",
            baseAddress,
            sizeOfHeaders,
            sizeOfImage,
            numberOfSections,
            entryPoint: decodedEntryPoint,
            titleId,
            titleName
        };
    }

    // ========================================================================
    // Standby Fallback 3D Canvas Renderer (if Wasm is loading or demo mode)
    // ========================================================================
    function initFallbackRenderer() {
        if (!state.canvas) return;
        const ctx = state.canvas.getContext('2d');
        if (!ctx) return;

        // Cube vertices in 3D
        const vertices = [
            [-1, -1,  1], [ 1, -1,  1], [ 1,  1,  1], [-1,  1,  1],
            [-1, -1, -1], [ 1, -1, -1], [ 1,  1, -1], [-1,  1, -1]
        ];

        const edges = [
            [0,1], [1,2], [2,3], [3,0],
            [4,5], [5,6], [6,7], [7,4],
            [0,4], [1,5], [2,6], [3,7]
        ];

        function render() {
            if (!state.isRunning) return;

            // Only render fallback if WebAssembly hasn't taken over the canvas
            if (!window._xbe_gpu || !window._xbe_gpu.isWebGPU) {
                state.rotation += 0.02;
                const w = state.canvas.width = state.canvas.clientWidth || 1280;
                const h = state.canvas.height = state.canvas.clientHeight || 720;

                ctx.fillStyle = "#030405";
                ctx.fillRect(0, 0, w, h);

                const rad = state.rotation;
                const cosY = Math.cos(rad);
                const sinY = Math.sin(rad);
                const cosX = Math.cos(rad * 0.7);
                const sinX = Math.sin(rad * 0.7);

                const projected = vertices.map(v => {
                    // Rotate Y
                    let x = v[0] * cosY + v[2] * sinY;
                    let z = -v[0] * sinY + v[2] * cosY;
                    let y = v[1];

                    // Rotate X
                    let y2 = y * cosX - z * sinX;
                    let z2 = y * sinX + z * cosX;

                    // Perspective
                    const fov = 3.2;
                    const scale = (fov / (fov + z2 + 2.0)) * (h * 0.35);
                    return [
                        x * scale + w / 2,
                        -y2 * scale + h / 2
                    ];
                });

                // Draw glowing Xbox lines
                ctx.shadowColor = "#00ff66";
                ctx.shadowBlur = 12;
                ctx.strokeStyle = "#00ff66";
                ctx.lineWidth = 2.5;

                ctx.beginPath();
                edges.forEach(edge => {
                    const p1 = projected[edge[0]];
                    const p2 = projected[edge[1]];
                    ctx.moveTo(p1[0], p1[1]);
                    ctx.lineTo(p2[0], p2[1]);
                });
                ctx.stroke();

                // Draw decorative center emblem
                ctx.shadowBlur = 20;
                ctx.fillStyle = "rgba(0, 255, 102, 0.15)";
                ctx.beginPath();
                ctx.arc(w / 2, h / 2, 45, 0, Math.PI * 2);
                ctx.fill();
            }

            // Update FPS
            state.frameCount++;
            const now = performance.now();
            if (now - state.lastFpsUpdate >= 500) {
                const fps = ((state.frameCount * 1000) / (now - state.lastFpsUpdate)).toFixed(1);
                if (state.fpsCounter) state.fpsCounter.innerText = fps;
                state.frameCount = 0;
                state.lastFpsUpdate = now;
            }

            requestAnimationFrame(render);
        }

        render();
    }

    // ========================================================================
    // Drag & Drop Setup
    // ========================================================================
    function setupDragAndDrop() {
        const dropzone = document.getElementById('dropzone');
        const fileInput = document.getElementById('file-input');
        if (!dropzone) return;

        dropzone.addEventListener('click', () => {
            if (fileInput) fileInput.click();
        });

        ['dragenter', 'dragover'].forEach(event => {
            dropzone.addEventListener(event, (e) => {
                e.preventDefault();
                dropzone.classList.add('dragover');
            });
        });

        ['dragleave', 'drop'].forEach(event => {
            dropzone.addEventListener(event, (e) => {
                e.preventDefault();
                dropzone.classList.remove('dragover');
            });
        });

        dropzone.addEventListener('drop', (e) => {
            if (e.dataTransfer && e.dataTransfer.files.length > 0) {
                loadFile(e.dataTransfer.files[0]);
            }
        });

        if (fileInput) {
            fileInput.addEventListener('change', (e) => {
                if (e.target.files.length > 0) {
                    loadFile(e.target.files[0]);
                }
            });
        }
    }

    function loadFile(file) {
        log(`Inspecting binary: ${file.name} (${(file.size / 1024).toFixed(1)} KB)...`, 'info');
        const reader = new FileReader();
        reader.onload = function (e) {
            try {
                const buffer = e.target.result;
                const info = parseXbeHeader(buffer);
                state.loadedXbe = info;

                log(`Title: ${info.titleName}`, 'info');
                log(`Title ID: 0x${info.titleId.toString(16).toUpperCase()}`, 'info');
                log(`Image Size: ${(info.sizeOfImage / 1024).toFixed(0)} KB | Sections: ${info.numberOfSections}`, 'info');
                log(`Decoded Entry Point: 0x${info.entryPoint.toString(16).toUpperCase()}`, 'info');

                if (state.titleBadge) state.titleBadge.innerText = info.titleName.toUpperCase();
                if (state.statusBadge) state.statusBadge.innerText = "LOADED";
                if (state.fileInfo) {
                    state.fileInfo.innerText = `Mounted: ${file.name} | Title ID: 0x${info.titleId.toString(16).toUpperCase()} | Size: ${(file.size / 1024).toFixed(1)} KB`;
                }

                // If Emscripten virtual filesystem is active, mount file
                if (typeof FS !== 'undefined' && FS.createDataFile) {
                    try {
                        FS.unlink('/default.xbe');
                    } catch (_) {}
                    FS.createDataFile('/', 'default.xbe', new Uint8Array(buffer), true, true, true);
                    log(`Executable mounted to VFS: /default.xbe`, 'info');
                }
            } catch (err) {
                log(`Failed to parse XBE: ${err.message}`, 'error');
            }
        };
        reader.readAsArrayBuffer(file);
    }

    // ========================================================================
    // Hardware WebGPU Capability Probe
    // ========================================================================
    async function probeWebGPU() {
        const badge = document.getElementById('gpu-badge');
        if (navigator.gpu) {
            try {
                const adapter = await navigator.gpu.requestAdapter();
                if (adapter) {
                    log(`Hardware WebGPU detected: ${adapter.info?.device || 'GPU Engine'}`, 'info');
                    if (badge) badge.innerText = "WebGPU: Supported";
                    return;
                }
            } catch (err) {
                console.warn(err);
            }
        }
        log("WebGPU not detected. Using Canvas 2D fallback engine.", 'warn');
        if (badge) badge.innerText = "Canvas: Accelerated";
    }

    // Initialize when DOM ready
    window.addEventListener('DOMContentLoaded', () => {
        setupDragAndDrop();
        initFallbackRenderer();
        probeWebGPU();
        log("WebRunner initialized. Drag an Xbox executable (.xbe) to begin.", 'info');
    });

    window.WebRunner = {
        state,
        log,
        loadFile
    };
})();
