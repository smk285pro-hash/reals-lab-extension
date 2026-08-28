/* Reals Lab — web UI (WebView2). Bridge: postMessage JSON <-> C++. */
'use strict';

// ============ i18n ============
const I18N = {
  vi: {
    'update.available': 'đã sẵn sàng — thêm Audio Lab', 'update.button': 'Update',
    'nav.market': 'Market', 'nav.audioLab': 'Audio Lab', 'nav.agent': 'Agent',
    'nav.browser': 'Browser', 'nav.account': 'Tài khoản',
    'market.search': 'Tìm plugin, script, VST...', 'market.trending': 'Đang thịnh hành',
    'market.installed': 'Đã cài', 'market.download': 'Tải về',
    'market.tagUpdate': 'CẬP NHẬT', 'market.installedNote': 'đã cài',
    'market.apiStub': 'Dữ liệu mẫu — API marketplace sẽ nối ở Phase 3',
    'market.chip.all': 'Tất cả', 'market.chip.effects': 'Effects', 'market.chip.midi': 'MIDI',
    'market.chip.utility': 'Utility', 'market.chip.scripts': 'Scripts', 'market.chip.free': 'Miễn phí',
    'lab.title': 'AUDIO LAB', 'lab.dropHint': 'Kéo file vào đây hoặc chọn từ Browser để phân tích',
    'lab.jobs': 'Tách stem · Lọc noise · Key & hợp âm · Tempo · Xuất MIDI',
    'lab.sub.stem': 'vocal · drum · bass · other', 'lab.sub.denoise': 'làm sạch audio',
    'lab.sub.keychord': 'phát hiện + MIDI', 'lab.sub.tempo': 'phát hiện BPM',
    'lab.apiStub': 'Chờ API phân tích — sẽ nối ở Phase 2',
    'agent.modes': 'CHẾ ĐỘ PHÉP QUYỀN', 'agent.mode1': 'Hỏi tất cả',
    'agent.mode2': 'Chỉ hỏi nguy hiểm', 'agent.mode3': 'Toàn quyền',
    'agent.hint': 'Ra lệnh cho agent... (VD: lọc noise all track audio)',
    'agent.apiStub': 'Phase 5 — cần API LLM từ server RealS',
    'browser.searchHint': 'Tìm trong thư mục...', 'browser.sort.name': 'Tên',
    'browser.sort.size': 'Dung lượng', 'browser.sort.date': 'Ngày sửa',
    'browser.audioOnly': 'Audio', 'browser.autoPreview': 'Auto', 'browser.favOnly': 'Yêu thích',
    'browser.tagFilter': 'Nhãn',
    'browser.parent': 'Thư mục cha', 'browser.searching': 'Đang tìm...',
    'browser.ctx.clearRecents': 'Xóa vừa mở',
    'browser.favorites': 'Yêu thích', 'browser.recents': 'Vừa mở',
    'browser.empty': 'Trống', 'browser.loop': 'Loop', 'browser.sendLab': 'Audio Lab',
    'browser.volume': 'Âm lượng',
    'browser.ctx.preview': 'Nghe thử',
    'browser.ctx.insert': 'Chèn vào project', 'browser.ctx.tag': 'Nhãn màu',
    'browser.tag.none': 'Không nhãn', 'browser.ctx.copyPath': 'Copy đường dẫn',
    'browser.ctx.reveal': 'Mở vị trí file', 'browser.ctx.rename': 'Đổi tên',
    'browser.ctx.delete': 'Xóa', 'browser.ctx.setRoot': 'Đặt làm thư mục gốc',
    'browser.ctx.openHere': 'Mở ở đây', 'browser.ctx.removeRoot': 'Xóa thư mục gốc này',
    'browser.lab.stem': 'Tách Stem', 'browser.lab.denoise': 'Lọc Noise',
    'browser.lab.keychord': 'Key & Hợp âm', 'browser.lab.tempo': 'Detect Tempo',
    'browser.lab.midi': 'Xuất MIDI', 'browser.rename.title': 'Đổi tên file:',
    'browser.delete.title': 'Xóa file này?', 'browser.delete.confirm': 'Xóa',
    'browser.pickRoot': 'Chọn hoặc thêm thư mục', 'browser.results': 'Kết quả tìm kiếm',
    'browser.dropTitle': 'Thêm thư mục gốc', 'browser.dropHint': 'Thả thư mục từ Windows Explorer vào đây để thêm vào Thư mục gốc',
    'settings.navPosition': 'Vị trí thanh điều hướng', 'settings.accent': 'Màu chủ đạo', 'settings.language': 'Ngôn ngữ',
    'settings.effects': 'Hiệu ứng', 'settings.noise': 'Lớp phủ noise',
    'settings.browser': 'Thư mục & Duyệt', 'settings.autoCollapse': 'Tự động thu gọn thư mục',
    'settings.displaySize': 'Kích thước hiển thị', 'size.small': 'Nhỏ (gọn)', 'size.medium': 'Vừa (chuẩn)', 'size.large': 'To (thoáng)',
    'pos.top': 'Trên', 'pos.bottom': 'Dưới', 'pos.left': 'Trái', 'pos.right': 'Phải',
    'accent.orange': 'Cam RealS', 'accent.amber': 'Hổ phách', 'accent.muted': 'Cam đất', 'accent.gray': 'Xám kim loại',
    'common.confirm': 'Xác nhận', 'common.cancel': 'Hủy', 'common.close': 'Đóng',
    'status.connected': 'Đã kết nối', 'status.disconnected': 'Mất kết nối',
    'account.notLogin': 'Chưa đăng nhập', 'account.loginHint': 'Đăng nhập bằng tài khoản reals.media',
    'account.login': 'Đăng nhập', 'account.apiStub': 'Phase 4 — chờ hệ thống đăng nhập trên web chính',
    'toast.copied': 'Đã copy đường dẫn', 'toast.notMedia': 'File này không phải media',
    'toast.inserted': 'Đã chèn vào project', 'toast.labQueued': 'Đã gửi tới Audio Lab (mở ở Phase 2)',
    'toast.decodeFail': 'Không đọc được file audio',
    'lab.noFile': 'Chưa chọn file — chọn trong Browser (click phải → Gửi Audio Lab)',
    'lab.apiLive': 'API thật: analyze · chords · separate · denoise',
    'toast.renamed': 'Đã đổi tên', 'toast.deleted': 'Đã xóa', 'toast.rootAdded': 'Đã thêm thư mục gốc',
    'toast.dropHint': 'Kéo thư mục vào đây để thêm gốc',
    'toast.notFolder': 'Kéo một thư mục (không phải file) để thêm gốc',
    'toast.renameFail': 'Đổi tên thất bại', 'toast.deleteFail': 'Xóa thất bại',
    'browser.refresh': 'Làm mới',
    'window.dock': 'Dock vào REAPER', 'window.undock': 'Tách thành cửa sổ riêng',
    'settings.window': 'Cửa sổ & Dock', 'settings.dockToReaper': 'Dock vào REAPER',
    'player.syncBpm': 'Sync BPM', 'player.keyTransposer': 'Chuyển Tone',
    'player.originalKey': 'Original Key', 'player.transposer': 'Bàn phím chuyển Tone',
    'player.semitones': 'bán cung', 'player.tags': 'Nhãn',
    'scanner.scanning': 'Đang quét...', 'scanner.complete': 'Quét hoàn tất',
    'scanner.cancelled': 'Đã hủy quét', 'scanner.starting': 'Đang bắt đầu quét...',
    'scanner.addedCount': 'Thêm mới',
    'scanner.cpuMode': 'Chế độ CPU',
    'scanner.cpuMode.low': 'Thấp (30%)', 'scanner.cpuMode.normal': 'Chuẩn (50%)',
    'scanner.cpuMode.high': 'Cao (85%)',
    'scanner.cpuMode.highWarn': 'Chế độ cao dùng nhiều CPU — tiếp tục?',
    'browser.similarTo': 'Tương tự', 'browser.matchPercent': 'khớp',
    'browser.noResults': 'Không tìm thấy mẫu tương tự',
    'browser.clearSimilar': 'Xóa bộ lọc tương tự',
    'browser.ctx.findSimilar': 'Tìm mẫu tương tự (AI)',
    'browser.ctx.scanNew': 'Quét file mới', 'browser.ctx.rescanAll': 'Quét lại tất cả',
    // Window / splitter / toast keys (MIN-01/02/07)
    'window.dockHint': 'Dock vào REAPER / Cửa sổ riêng', 'window.settingsHint': 'Cài đặt',
    'window.minimize': 'Thu nhỏ', 'window.maximize': 'Phóng to / Khôi phục',
    'window.close': 'Đóng', 'browser.toggleTree': 'Ẩn/Hiện Cây thư mục',
    'splitter.tree': 'Kéo để chỉnh độ rộng Cây thư mục',
    'splitter.preview': 'Kéo để chỉnh chiều cao Trình phát',
    'toast.labError': 'Lỗi Lab', 'toast.scannerError': 'Lỗi quét',
    'toast.similarError': 'Lỗi tìm mẫu tương tự',
    'sync.noBpm': 'Sync: không tìm thấy BPM, thử 120',
    'lab.alreadyRunning': 'Đang có job chạy — chờ xong đã nhé',
  },
  en: {
    'update.available': 'is ready — Audio Lab added', 'update.button': 'Update',
    'nav.market': 'Market', 'nav.audioLab': 'Audio Lab', 'nav.agent': 'Agent',
    'nav.browser': 'Browser', 'nav.account': 'Account',
    'market.search': 'Search plugins, scripts, VST...', 'market.trending': 'Trending',
    'market.installed': 'Installed', 'market.download': 'Download',
    'market.tagUpdate': 'UPDATE', 'market.installedNote': 'installed',
    'market.apiStub': 'Sample data — marketplace API lands in Phase 3',
    'market.chip.all': 'All', 'market.chip.effects': 'Effects', 'market.chip.midi': 'MIDI',
    'market.chip.utility': 'Utility', 'market.chip.scripts': 'Scripts', 'market.chip.free': 'Free',
    'lab.title': 'AUDIO LAB', 'lab.dropHint': 'Drop files here or pick from Browser to analyze',
    'lab.jobs': 'Stem split · Denoise · Key & chords · Tempo · MIDI export',
    'lab.sub.stem': 'vocal · drum · bass · other', 'lab.sub.denoise': 'clean up audio',
    'lab.sub.keychord': 'detect + MIDI', 'lab.sub.tempo': 'detect BPM',
    'lab.apiStub': 'Waiting for analysis API — Phase 2',
    'agent.modes': 'PERMISSION MODE', 'agent.mode1': 'Ask all',
    'agent.mode2': 'Ask dangerous only', 'agent.mode3': 'Full control',
    'agent.hint': 'Command the agent... (e.g. denoise all audio tracks)',
    'agent.apiStub': 'Phase 5 — needs RealS server LLM API',
    'browser.searchHint': 'Search in folder...', 'browser.sort.name': 'Name',
    'browser.sort.size': 'Size', 'browser.sort.date': 'Date',
    'browser.audioOnly': 'Audio', 'browser.autoPreview': 'Auto', 'browser.favOnly': 'Favorites',
    'browser.tagFilter': 'Tag',
    'browser.parent': 'Parent folder', 'browser.searching': 'Searching...',
    'browser.ctx.clearRecents': 'Clear recents',
    'browser.favorites': 'Favorites', 'browser.recents': 'Recent',
    'browser.empty': 'Empty', 'browser.loop': 'Loop', 'browser.sendLab': 'Audio Lab',
    'browser.volume': 'Volume',
    'browser.ctx.preview': 'Preview',
    'browser.ctx.insert': 'Insert into project', 'browser.ctx.tag': 'Color tag',
    'browser.tag.none': 'No tag', 'browser.ctx.copyPath': 'Copy path',
    'browser.ctx.reveal': 'Reveal in Explorer', 'browser.ctx.rename': 'Rename',
    'browser.ctx.delete': 'Delete', 'browser.ctx.setRoot': 'Set as root',
    'browser.ctx.openHere': 'Open here', 'browser.ctx.removeRoot': 'Remove this root',
    'browser.lab.stem': 'Split Stem', 'browser.lab.denoise': 'Denoise',
    'browser.lab.keychord': 'Key & Chords', 'browser.lab.tempo': 'Detect Tempo',
    'browser.lab.midi': 'Export MIDI', 'browser.rename.title': 'Rename file:',
    'browser.delete.title': 'Delete this file?', 'browser.delete.confirm': 'Delete',
    'browser.pickRoot': 'Select or add folder', 'browser.results': 'Search results',
    'browser.dropTitle': 'Add Root Folder', 'browser.dropHint': 'Drop folder from Windows Explorer here to add as root',
    'settings.navPosition': 'Navigation Position', 'settings.accent': 'Accent Color', 'settings.language': 'Language',
    'settings.effects': 'Effects', 'settings.noise': 'Noise overlay',
    'settings.browser': 'Folder tree', 'settings.autoCollapse': 'Auto-collapse folders',
    'settings.displaySize': 'Display density', 'size.small': 'Compact', 'size.medium': 'Standard', 'size.large': 'Large',
    'pos.top': 'Top', 'pos.bottom': 'Bottom', 'pos.left': 'Left', 'pos.right': 'Right',
    'accent.orange': 'RealS Orange', 'accent.amber': 'Amber', 'accent.muted': 'Muted Orange', 'accent.gray': 'Metal Gray',
    'common.confirm': 'Confirm', 'common.cancel': 'Cancel', 'common.close': 'Close',
    'status.connected': 'Connected', 'status.disconnected': 'Disconnected',
    'account.notLogin': 'Not logged in', 'account.loginHint': 'Sign in with your reals.media account',
    'account.login': 'Sign in', 'account.apiStub': 'Phase 4 — waiting for web login system',
    'toast.copied': 'Path copied', 'toast.notMedia': 'Not a media file',
    'toast.inserted': 'Inserted into project', 'toast.labQueued': 'Sent to Audio Lab (opens in Phase 2)',
    'toast.decodeFail': 'Cannot decode audio file',
    'lab.noFile': 'No file selected — pick in Browser (right-click → Send to Audio Lab)',
    'lab.apiLive': 'Real API: analyze · chords · separate · denoise',
    'toast.renamed': 'Renamed', 'toast.deleted': 'Deleted', 'toast.rootAdded': 'Root added',
    'toast.dropHint': 'Drop a folder here to add a root',
    'toast.notFolder': 'Drop a folder (not a file) to add a root',
    'toast.renameFail': 'Rename failed', 'toast.deleteFail': 'Delete failed',
    'browser.refresh': 'Refresh',
    'window.dock': 'Dock to REAPER', 'window.undock': 'Undock window',
    'settings.window': 'Window & Dock', 'settings.dockToReaper': 'Dock to REAPER',
    'player.syncBpm': 'Sync BPM', 'player.keyTransposer': 'Key Transposer',
    'player.originalKey': 'Original Key', 'player.transposer': 'Tone Transposer',
    'player.semitones': 'semitones', 'player.tags': 'Tags',
    'scanner.scanning': 'Scanning...', 'scanner.complete': 'Scan complete',
    'scanner.cancelled': 'Scan cancelled', 'scanner.starting': 'Starting scan...',
    'scanner.addedCount': 'Added',
    'scanner.cpuMode': 'CPU mode',
    'scanner.cpuMode.low': 'Low (30%)', 'scanner.cpuMode.normal': 'Normal (50%)',
    'scanner.cpuMode.high': 'High (85%)',
    'scanner.cpuMode.highWarn': 'High mode uses a lot of CPU — continue?',
    'browser.similarTo': 'Similar to', 'browser.matchPercent': 'match',
    'browser.noResults': 'No similar samples found',
    'browser.clearSimilar': 'Clear similar filter',
    'browser.ctx.findSimilar': 'Find similar (AI)',
    'browser.ctx.scanNew': 'Scan new files', 'browser.ctx.rescanAll': 'Rescan all',
    // Window / splitter / toast keys (MIN-01/02/07)
    'window.dockHint': 'Dock into REAPER / Float window', 'window.settingsHint': 'Settings',
    'window.minimize': 'Minimize', 'window.maximize': 'Maximize / Restore',
    'window.close': 'Close', 'browser.toggleTree': 'Show/hide folder tree',
    'splitter.tree': 'Drag to resize the folder tree',
    'splitter.preview': 'Drag to resize the player',
    'toast.labError': 'Lab error', 'toast.scannerError': 'Scanner error',
    'toast.similarError': 'Error finding similar samples',
    'sync.noBpm': 'Sync: no BPM found, trying 120',
    'lab.alreadyRunning': 'A job is already running — please wait',
  },
};
let LANG = 'vi';
const tr = (k) => (I18N[LANG] && I18N[LANG][k]) || I18N.vi[k] || k;

// ============ Bridge (with Standalone Browser Mock Fallback) ============
const hasWebView = !!(window.chrome && window.chrome.webview && window.chrome.webview.postMessage);

const mockStore = {
  config: { language: 'vi', navPosition: 'top', accent: 'orange', noiseOverlay: true },
  roots: [
    { name: 'Samples & Loops', path: 'D:\\Samples' },
    { name: 'Projects', path: 'D:\\ReaperProjects' }
  ],
  files: [
    { name: 'Kick_Punchy_01.wav', path: 'D:\\Samples\\Kick_Punchy_01.wav', isAudio: true, size: 245000, modified: 1787500000 },
    { name: 'Snare_808_Clean.wav', path: 'D:\\Samples\\Snare_808_Clean.wav', isAudio: true, size: 182000, modified: 1787510000 },
    { name: 'HiHat_Trap_Closed.wav', path: 'D:\\Samples\\HiHat_Trap_Closed.wav', isAudio: true, size: 94000, modified: 1787520000 },
    { name: 'Vocal_Hook_128bpm_Am.wav', path: 'D:\\Samples\\Vocal_Hook_128bpm_Am.wav', isAudio: true, size: 1420000, modified: 1787530000 },
    { name: 'Bass_Sub_Deep_F.wav', path: 'D:\\Samples\\Bass_Sub_Deep_F.wav', isAudio: true, size: 850000, modified: 1787540000 },
    { name: 'Chord_Loop_Piano_120bpm.wav', path: 'D:\\Samples\\Chord_Loop_Piano_120bpm.wav', isAudio: true, size: 2100000, modified: 1787550000 },
    { name: 'Synth_Lead_Drop.wav', path: 'D:\\Samples\\Synth_Lead_Drop.wav', isAudio: true, size: 1650000, modified: 1787560000 },
    { name: 'Drums', path: 'D:\\Samples\\Drums', isAudio: false, isDir: true, size: 0, modified: 1787500000 },
    { name: 'Readme_Sample_Pack.txt', path: 'D:\\Samples\\Readme_Sample_Pack.txt', isAudio: false, isDir: false, size: 1200, modified: 1787500000 }
  ],
  favorites: ['D:\\Samples\\Kick_Punchy_01.wav', 'D:\\Samples\\Vocal_Hook_128bpm_Am.wav'],
  recents: ['D:\\Samples\\Snare_808_Clean.wav', 'D:\\Samples\\Bass_Sub_Deep_F.wav'],
  tags: { 'D:\\Samples\\Kick_Punchy_01.wav': 3, 'D:\\Samples\\Vocal_Hook_128bpm_Am.wav': 5 }
};

function mockBridge(cmd, args = {}) {
  return new Promise((resolve) => {
    setTimeout(() => {
      if (cmd === 'app.info') resolve({ version: '0.2.0 (Browser Preview)', platform: 'browser' });
      else if (cmd === 'config.getAll') resolve({ ...mockStore.config });
      else if (cmd === 'config.set') {
        if (args.key) mockStore.config[args.key] = args.value;
        resolve({ ok: true });
      } else if (cmd === 'fs.roots') resolve(mockStore.roots);
      else if (cmd === 'fs.addRoot') {
        let p = (args.path || '').replace(/\//g, '\\');
        if (p.length > 3 && p.endsWith('\\')) p = p.replace(/[\\]+$/, '');
        let name = args.name || '';
        if (/\.[a-zA-Z0-9]+$/.test(p)) {
          const lastSlash = p.lastIndexOf('\\');
          if (lastSlash > 0) p = p.slice(0, lastSlash);
          name = '';
        }
        if (!name || name === args.path || /\.[a-zA-Z0-9]+$/.test(name)) {
          name = p.split(/[\\/]/).filter(Boolean).pop() || p;
        }
        if (!mockStore.roots.some((r) => r.path === p)) {
          mockStore.roots.push({ name, path: p });
        }
        resolve({ ok: true });
      } else if (cmd === 'fs.removeRoot') {
        mockStore.roots = mockStore.roots.filter((r) => r.path !== args.path);
        resolve({ ok: true });
      } else if (cmd === 'fs.subdirs') resolve(['Drums', 'Synths', 'Bass', 'Vocals', 'FX']);
      else if (cmd === 'fs.list') {
        const p = (args.path || '').replace(/\//g, '\\');
        if (/\\drums$/i.test(p)) {
          resolve([
            { name: 'Kick_Room.wav', path: p + '\\Kick_Room.wav', isAudio: true, isDir: false, size: 180000, modified: 1787500000 },
            { name: 'Snare_Rim.wav', path: p + '\\Snare_Rim.wav', isAudio: true, isDir: false, size: 122000, modified: 1787510000 },
            { name: 'Hat_Closed.wav', path: p + '\\Hat_Closed.wav', isAudio: true, isDir: false, size: 64000, modified: 1787520000 },
          ]);
        } else {
          resolve(mockStore.files.map((f) => ({ isDir: !!f.isDir, ...f })));
        }
      }
      else if (cmd === 'fs.watch') resolve({ ok: true });
      else if (cmd === 'fs.dropPaths') {
        const added = [];
        (args.paths || []).forEach((raw) => {
          let p = (raw || '').replace(/\//g, '\\');
          if (p.length > 3 && p.endsWith('\\')) p = p.replace(/[\\]+$/, '');
          if (/\.[a-zA-Z0-9]+$/.test(p)) {
            const lastSlash = p.lastIndexOf('\\');
            if (lastSlash > 0) p = p.slice(0, lastSlash);
          }
          const name = p.split(/[\\/]/).filter(Boolean).pop() || p;
          if (p && !mockStore.roots.some((r) => r.path === p)) {
            mockStore.roots.push({ name, path: p });
            added.push({ name, path: p });
          }
        });
        setTimeout(() => handleEvent('fs.rootsChanged', { added }), 20);
        resolve({ ok: true, added });
      }
      else if (cmd === 'browser.favorites') resolve(mockStore.favorites);
      else if (cmd === 'browser.recents') resolve(mockStore.recents);
      else if (cmd === 'browser.tags') {
        if (args.path) resolve({ ofPath: mockStore.tags[args.path] || 0 });
        else resolve({ tags: { ...mockStore.tags } });
      }
      else if (cmd === 'browser.toggleFavorite') {
        const idx = mockStore.favorites.indexOf(args.path);
        if (idx >= 0) mockStore.favorites.splice(idx, 1);
        else mockStore.favorites.push(args.path);
        resolve(idx < 0);
      } else if (cmd === 'browser.tag') {
        mockStore.tags[args.path] = args.color;
        resolve({ ok: true });
      } else if (cmd === 'browser.search') {
        const q = (args.query || '').toLowerCase();
        const gen = args.gen || (mockStore._searchGen = (mockStore._searchGen || 0) + 1);
        const results = mockStore.files.filter((f) => !f.isDir && f.name.toLowerCase().includes(q));
        setTimeout(() => handleEvent('browser.searchResult', { gen, results }), 20);
        resolve({ pending: true, gen });
      } else if (cmd === 'browser.clearRecents') {
        mockStore.recents = [];
        resolve({ ok: true });
      } else if (cmd === 'audio.probe') {
        resolve({ duration: 1.8, sampleRate: 44100, channels: 2, ok: true });
      } else if (cmd === 'audio.seek' || cmd === 'browser.beginDrag') {
        resolve({ ok: true });
      } else if (cmd === 'audio.setPitchShift') {
        const semitones = args.semitones || 0;
        resolve({ ok: true, pitchSemitones: semitones });
      } else if (cmd === 'audio.setSyncBpm') {
        const enabled = !!args.enabled;
        const bpm = args.bpm || 120;
        const sampleBpm = args.sampleBpm || 120;
        const ratio = enabled ? (bpm / (sampleBpm || 120)) : 1.0;
        resolve({ ok: true, syncBpm: enabled, ratio, projectBpm: bpm, sampleBpm });
      } else if (cmd === 'audio.detectBpm') {
        const p = args.path || '';
        const m = p.match(/(\d{2,3})\s*bpm/i);
        const bpm = m ? parseFloat(m[1]) : 120;
        resolve({ ok: true, bpm, path: p });
      } else if (cmd === 'audio.getSampleMeta') {
        const p = args.path || '';
        const m = p.match(/(\d{2,3})\s*bpm/i);
        const bpm = m ? parseFloat(m[1]) : 0;
        const km = p.match(/_([A-G][#b]?(?:m|maj|min|minor|major)?)(?:_|\.|$)/i);
        const key = km ? km[1].toUpperCase() : '';
        resolve({ ok: true, bpm, key, genre: '', mood: '', path: p });
      } else if (cmd === 'audio.setOriginalKey' || cmd === 'audio.resetPitch') {
        resolve({ ok: true, pitchSemitones: 0 });
      } else if (cmd === 'ai.analyzeFile') {
        resolve({
          ok: true,
          analysis: {
            tempo: { bpm: 124.0, confidence: 0.95, method: 'tempo_cnn' },
            key: { key: 'F#', mode: 'Minor', camelot: '11A', openKey: '4m', confidence: 0.92 },
            genres: [{ tag: 'Trap-EDM', score: 0.88 }, { tag: 'Future Bass', score: 0.74 }],
            moods: [{ tag: 'dark', score: 0.85 }, { tag: 'aggressive', score: 0.78 }],
            embeddingDim: 512
          }
        });
      } else if (cmd === 'search.findSimilar' || cmd === 'ai.findSimilar') {
        const results = mockStore.files.filter((f) => !f.isDir).map((f, i) => ({
          ...f,
          score: 0.95 - (i * 0.05),
          similarity: Math.round((0.95 - (i * 0.05)) * 100),
          bpm: 120,
          key: 'F#',
          mode: 'Minor',
          genre: 'Trap-EDM',
          mood: 'dark'
        }));
        resolve({ ok: true, results, count: results.length });
      } else if (cmd === 'ai.searchSemantic') {
        const q = (args.query || '').toLowerCase();
        const results = mockStore.files.filter((f) => !f.isDir).map((f) => ({
          ...f,
          score: 0.85,
          bpm: 120,
          key: 'F#',
          mode: 'Minor',
          genre: 'Trap-EDM',
          mood: 'dark'
        }));
        resolve({ ok: true, results, count: results.length });
      } else if (cmd === 'db.search') {
        const results = mockStore.files.filter((f) => !f.isDir).map((f) => ({
          ...f,
          bpm: 120,
          key: 'F#',
          mode: 'Minor',
          genre: 'Trap-EDM',
          mood: 'dark',
          aiAnalyzed: true
        }));
        resolve({ ok: true, results, count: results.length });
      } else if (cmd === 'scanner.start') {
        resolve({ ok: true, jobId: 1, isScanning: true, rootsCount: (args.roots || []).length });
      } else if (cmd === 'scanner.status') {
        resolve({ ok: true, isScanning: false, total: mockStore.files.length, processed: mockStore.files.length, added: mockStore.files.length, skipped: 0, errors: 0, currentFile: '' });
      } else if (cmd === 'reaper.tempo') {
        resolve({ bpm: 120 });
      } else if (cmd === 'reaper.insert' || cmd === 'reaper.insertMany') {
        const pr = args.playrate || args.ratio || 1.0;
        const synced = Math.abs(pr - 1.0) > 0.001;
        resolve({ ok: true, playrate: pr, synced });
      } else if (cmd === 'audio.play') {
        if (args.path) {
          mockStore.recents = [args.path, ...mockStore.recents.filter((r) => r !== args.path)].slice(0, 20);
        }
        const fakeEnv = [];
        for (let i = 0; i < 80; ++i) fakeEnv.push(Math.abs(Math.sin(i * 0.15)) * (0.3 + Math.random() * 0.7));
        resolve({ duration: 4.8, sampleRate: 44100, channels: 2, envelope: fakeEnv });
      } else if (cmd === 'audio.stop') resolve({ ok: true });
      else if (cmd === 'audio.setVolume') resolve({ ok: true });
      else resolve({ ok: true });
    }, 10);
  });
}

let _bridgeId = 0;
const _pending = new Map();
// Bridge health probe — flips the status-bar dot when WebView2 stops responding.
let _bridgeHealthOk = null;
function setBridgeHealth(ok) {
  if (_bridgeHealthOk === ok) return;
  _bridgeHealthOk = ok;
  state.bridgeOk = ok;
  const dot = document.querySelector('#statusbar .conn i');
  const txt = document.querySelector('#statusbar .conn span');
  if (dot) dot.style.background = ok ? 'var(--free-tx)' : 'var(--danger)';
  if (txt) txt.textContent = tr(ok ? 'status.connected' : 'status.disconnected');
}

function bridge(cmd, args = {}) {
  if (!hasWebView) {
    return mockBridge(cmd, args).then(
      (v) => { setBridgeHealth(true); return v; },
      (e) => { setBridgeHealth(false); throw e; }
    );
  }
  return new Promise((resolve, reject) => {
    const id = ++_bridgeId;
    const timer = setTimeout(() => {
      if (_pending.has(id)) {
        _pending.delete(id);
        setBridgeHealth(false);
        reject(new Error(`Bridge command '${cmd}' timed out`));
      }
    }, 15000);
    _pending.set(id, {
      resolve: (data) => { clearTimeout(timer); setBridgeHealth(true); resolve(data); },
      reject: (err) => { clearTimeout(timer); setBridgeHealth(false); reject(err); }
    });
    try {
      window.chrome.webview.postMessage({ id, cmd, args });
    } catch (e) {
      clearTimeout(timer);
      _pending.delete(id);
      setBridgeHealth(false);
      reject(e);
    }
  });
}

if (hasWebView) {
  window.chrome.webview.addEventListener('message', (e) => {
    const m = e.data;
    if (m && m.event) { handleEvent(m.event, m.data); return; }
    if (m && m.id && _pending.has(m.id)) {
      const p = _pending.get(m.id);
      _pending.delete(m.id);
      m.ok ? p.resolve(m.data) : p.reject(new Error(m.error || 'bridge error'));
    }
  });
}

// ============ Helpers ============
const $ = (s) => document.querySelector(s);
const $$ = (s) => document.querySelectorAll(s);
function el(tag, cls, text) {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text !== undefined) e.textContent = text;
  return e;
}
function fmtSize(b) {
  if (b >= 1073741824) return (b / 1073741824).toFixed(1) + ' GB';
  if (b >= 1048576) return (b / 1048576).toFixed(1) + ' MB';
  if (b >= 1024) return (b / 1024).toFixed(0) + ' KB';
  return b + ' B';
}
function fmtTime(epoch) {
  const d = new Date(epoch * 1000);
  const p = (n) => String(n).padStart(2, '0');
  return `${p(d.getDate())}/${p(d.getMonth() + 1)}/${d.getFullYear()} ${p(d.getHours())}:${p(d.getMinutes())}`;
}
function fmtDur(sec) {
  if (!sec || sec <= 0) return '';
  const m = Math.floor(sec / 60);
  const s = Math.floor(sec % 60);
  return m + ':' + String(s).padStart(2, '0');
}
function parentDir(path) {
  if (!path) return path;
  const sep = path.includes('\\') ? '\\' : '/';
  const trimmed = path.endsWith(sep) ? path.slice(0, -1) : path;
  const i = trimmed.lastIndexOf(sep);
  return i <= 0 ? trimmed : trimmed.slice(0, i);
}
function toast(msg) {
  const t = $('#toast');
  if (!t) return;
  t.textContent = msg;
  t.classList.remove('hidden');
  clearTimeout(t._h);
  // Scale duration by text length: 2s minimum, +60ms per char, capped at 6s.
  const dur = Math.min(6000, Math.max(2000, msg.length * 60));
  t._h = setTimeout(() => t.classList.add('hidden'), dur);
}
function applyI18n() {
  $$('[data-i18n]').forEach((e) => (e.textContent = tr(e.dataset.i18n)));
  $$('[data-i18n-ph]').forEach((e) => (e.placeholder = tr(e.dataset.i18nPh)));
  // MAJ-03: tooltips switch language too — [data-i18n-title] used to be
  // ignored, so title attributes stayed in the initial language forever.
  $$('[data-i18n-title]').forEach((e) => (e.title = tr(e.dataset.i18nTitle)));
}

const TAG_COLORS = [null, '#B0B4BB', '#FF5C66', '#FF6B2C', '#F5C542', '#35D07F', '#55A5FF', '#B98CFF'];
// Full accent sets — mockup .acc-* classes override all five accent tokens,
// not just --accent, so hover/soft/border stay in the same hue family.
const ACCENTS = {
  orange: { accent: '#FF6B2C', hover: '#FF7A3D', active: '#E9571D', rgb: '255,107,44' },
  amber: { accent: '#F09A2E', hover: '#F5A945', active: '#D9821A', rgb: '240,154,46' },
  muted: { accent: '#D96E30', hover: '#E37E42', active: '#C05F26', rgb: '217,110,48' },
  gray: { accent: '#B4B8BF', hover: '#CDD1D7', active: '#9A9EA6', rgb: '180,184,191' },
};

// ============ State ============
const state = {
  roots: [], currentDir: null, selected: null, playingPath: null,
  envelope: [], duration: 0, position: 0, peak: 0, playing: false,
  loop: false, sort: 0, audioOnly: false, expanded: new Set(), tab: 'browser',
  tagCache: {}, favSet: new Set(),
  listSeq: 0, searchSeq: 0, treeSeq: 0,
  autoPreview: true, favOnly: false, tagFilter: 0,
  rawFiles: [], files: [], listDir: null,
  searchQ: '', searchPending: false, searchGen: 0,
  similarSource: null, similarSourceName: null,
  probeCache: {}, probeInflight: new Set(),
  syncBpm: false, pitchSemitones: 0,
  sampleBpm: 0, sampleKey: 'ORIGINAL', sampleMode: '',
  sampleTags: [],
  autoCollapseTree: true,
  displaySize: 'medium',
  subCache: {},
  dirScrolls: {},
  _dragArm: null,
  _suppressClick: false,
  _dropHoverT: null,
  _watchT: null,
  _previewArmT: null,
  _accentCache: '#FF6B2C',
  bridgeOk: null,
};

// ============ Events from C++ ============
function handleEvent(event, data) {
  if (event === 'toast') { toast(data.text || ''); return; }
  if (event === 'window.state') {
    if (data) {
      $('#app')?.classList.toggle('maximized', !!data.maximized);
      const btnMax = $('#btnWinMax');
      if (btnMax) btnMax.textContent = data.maximized ? '❐' : '▢';
    }
    return;
  }
  if (event === 'window.dockState') {
    if (data) applyDockState(!!data.docked);
    return;
  }
  if (event === 'lab.progress') { labProgress(true, data.percent, data.stage); return; }
  if (event === 'lab.result') { renderLabResult(data); return; }
  if (event === 'lab.error') { labState.running = false; labProgress(false); toast(tr('toast.labError') + (data.error ? ': ' + data.error : '')); return; }
  if (event === 'scanner.progress') {
    const bar = $('#scannerBar');
    const status = $('#scannerStatusText');
    const file = $('#scannerFileText');
    const fill = $('#scannerProgressFill');
    if (!data || !bar) return;

    if (data.isCancelled) {
      if (status) status.textContent = tr('scanner.cancelled');
      if (fill) fill.style.width = '0%';
      setTimeout(() => { if (bar) bar.classList.add('hidden'); }, 2000);
      return;
    }

    if (data.isComplete) {
      if (status) status.textContent = `${tr('scanner.complete')} (${data.processed || 0})`;
      if (file) file.textContent = '';
      if (fill) fill.style.width = '100%';
      toast(`${tr('scanner.complete')} (${data.processed || 0})`);
      setTimeout(() => { if (bar) bar.classList.add('hidden'); }, 3000);
      if (state.currentDir) loadDir(state.currentDir, false);
      return;
    }

    bar.classList.remove('hidden');
    const total = data.total || 0;
    const proc = data.processed || 0;
    const pct = total > 0 ? Math.min(100, Math.round((proc / total) * 100)) : 10;
    if (status) status.textContent = `${tr('scanner.scanning')} ${proc}/${total} (${tr('scanner.addedCount')}: ${data.added || 0})`;
    if (file) file.textContent = data.currentFile || '';
    if (fill) fill.style.width = `${pct}%`;
    return;
  }
  if (event === 'browser.searchResult') {
    if (data.gen !== state.searchGen) return;
    if (!(state.searchQ || '').trim()) return;
    state.searchPending = false;
    state.rawFiles = data.results || [];
    state.listDir = null;
    paintFromRaw();
    probeVisibleAudio();
    return;
  }
  if (event === 'fs.dropHover') {
    const overlay = $('#dropOverlay');
    if (!overlay) return;
    clearTimeout(state._dropHoverT);
    if (data && data.on) {
      overlay.classList.remove('hidden');
    } else {
      // Child HWND handoff fires Leave then Enter; don't flicker the overlay.
      state._dropHoverT = setTimeout(() => overlay.classList.add('hidden'), 80);
    }
    return;
  }
  if (event === 'fs.rootsChanged') {
    const overlay = $('#dropOverlay');
    if (overlay) overlay.classList.add('hidden');
    const added = (data && data.added) || [];
    refreshRoots().then(() => {
      renderTree();
      if (added.length) {
        toast(tr('toast.rootAdded') + ': ' + (added[0].name || ''));
        if (state.tab !== 'browser') {
          state.tab = 'browser';
          renderNav();
          showTab('browser');
        }
        if (added[0].path && state.currentDir !== added[0].path) openDir(added[0].path);
      } else {
        toast(tr('toast.notFolder'));
      }
    });
    return;
  }
  if (event === 'fs.changed') {
    const dir = data.path;
    clearTimeout(state._watchT);
    state._watchT = setTimeout(() => {
      delete state.subCache[dir];
      if (dir === state.currentDir && !(state.searchQ || '').trim())
        loadDir(state.currentDir, true);
    }, 250);
    return;
  }
  if (event === 'audio.envelope') {
    if (state.playingPath === data.path || state.selected === data.path) {
      state.envelope = data.envelope || [];
      drawWaveform();
    }
    return;
  }
  if (event === 'audio.syncState') {
    if (data) {
      if (typeof data.syncBpm === 'boolean') {
        state.syncBpm = data.syncBpm;
        $('#btnSyncBpm')?.classList.toggle('on', state.syncBpm);
      }
      if (typeof data.semitones === 'number') {
        state.pitchSemitones = data.semitones;
        const badge = $('#pitchShiftBadge');
        if (badge) {
          badge.textContent = semitonesToDisplay(data.semitones);
          badge.classList.toggle('active', data.semitones !== 0);
        }
        $('#btnKeyTransposer')?.classList.toggle('shifted', data.semitones !== 0);
        const semitoneLabel = $('#pianoSemitoneLabel');
        if (semitoneLabel) {
          semitoneLabel.textContent = (data.semitones > 0 ? '+' : '') + data.semitones + ' ' + tr('player.semitones');
        }
        const keyLabel = $('#playerKeyLabel');
        if (keyLabel) {
          keyLabel.textContent = calculateTransposedKey(state.sampleKey, data.semitones);
        }
      }
    }
    return;
  }
  if (event === 'audio.state') {
    state.position = data.position || 0;
    state.peak = data.peak || 0;
    state.playing = !!data.playing;
    state.duration = data.duration || state.duration;
    if (typeof data.pitchSemitones === 'number') {
      state.pitchSemitones = data.pitchSemitones;
    }
    // Keep the play button in sync when playback stops on its own.
    const bp = $('#btnPlay');
    if (bp) bp.textContent = state.playing ? 'II' : '>';
    updatePreviewLive();
  }
}

// ============ Piano & Tag Helpers ============
const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

function semitonesToDisplay(semitones) {
  if (semitones === 0) return '0st';
  return (semitones > 0 ? '+' : '') + semitones + 'st';
}

function semitoneToNote(semitones) {
  const norm = ((semitones % 12) + 12) % 12;
  return NOTE_NAMES[norm];
}

function calculateTransposedKey(originalKey, semitones) {
  if (!originalKey || originalKey === 'ORIGINAL' || originalKey === 'UNKNOWN') {
    if (semitones === 0) return 'ORIGINAL';
    return (semitones > 0 ? '+' : '') + semitones + 'st';
  }
  const rootMatch = originalKey.match(/^([A-G][#b]?)(.*)$/i);
  if (!rootMatch) return (semitones > 0 ? '+' : '') + semitones + 'st';
  let root = rootMatch[1].toUpperCase();
  if (root === 'DB') root = 'C#';
  else if (root === 'EB') root = 'D#';
  else if (root === 'GB') root = 'F#';
  else if (root === 'AB') root = 'G#';
  else if (root === 'BB') root = 'A#';
  const rest = rootMatch[2];
  const rootIdx = NOTE_NAMES.indexOf(root);
  if (rootIdx < 0) return (semitones > 0 ? '+' : '') + semitones + 'st';
  const newIdx = ((rootIdx + semitones) % 12 + 12) % 12;
  return NOTE_NAMES[newIdx] + (rest ? ' ' + rest.trim() : '');
}

function renderPlayerTags(tags) {
  const bar = $('#playerTagBar');
  if (!bar) return;
  bar.innerHTML = '';
  if (!tags || !tags.length) {
    bar.classList.add('hidden');
    return;
  }
  bar.classList.remove('hidden');
  tags.forEach((t) => {
    const chip = document.createElement('span');
    const typeClass = t.type ? `tag-${t.type}` : 'tag-genre';
    chip.className = `tag-chip ${typeClass}`;
    chip.textContent = t.name || t.tag || t;
    chip.onclick = (e) => {
      e.stopPropagation();
      const tagText = t.name || t.tag || t;
      const searchInput = $('#search');
      if (searchInput) {
        searchInput.value = '/' + tagText;
        state.searchQ = searchInput.value;
        runSearch(state.searchQ);
      }
    };
    bar.appendChild(chip);
  });
}

function extractTagsFromFilename(name) {
  const tags = [];
  const lower = (name || '').toLowerCase();
  if (lower.includes('kick') || lower.includes('snare') || lower.includes('hat') || lower.includes('drum')) {
    tags.push({ name: 'Drums', type: 'inst' });
  }
  if (lower.includes('vocal') || lower.includes('hook') || lower.includes('chant') || lower.includes('choir') || lower.includes('male')) {
    tags.push({ name: 'Vocals', type: 'inst' });
    if (lower.includes('choir')) tags.push({ name: 'Choir', type: 'prop' });
    if (lower.includes('male')) tags.push({ name: 'Male', type: 'prop' });
  }
  if (lower.includes('trap')) tags.push({ name: 'Trap', type: 'genre' });
  if (lower.includes('edm') || lower.includes('future')) tags.push({ name: 'EDM', type: 'genre' });
  if (lower.includes('soul') || lower.includes('jazz')) tags.push({ name: 'Soul', type: 'genre' });
  if (lower.includes('piano') || lower.includes('chord')) tags.push({ name: 'Piano', type: 'inst' });
  if (lower.includes('bass') || lower.includes('sub') || lower.includes('808')) tags.push({ name: 'Bass', type: 'inst' });
  if (lower.includes('clean') || lower.includes('dry')) tags.push({ name: 'Clean', type: 'prop' });
  if (lower.includes('reverb') || lower.includes('wet') || lower.includes('space')) tags.push({ name: 'Reverb', type: 'prop' });
  if (lower.includes('ensemble') || lower.includes('stack')) tags.push({ name: 'Ensemble', type: 'prop' });
  if (lower.includes('dark') || lower.includes('deep')) tags.push({ name: 'Dark', type: 'mood' });
  if (lower.includes('happy') || lower.includes('uplift')) tags.push({ name: 'Happy', type: 'mood' });
  return tags;
}

async function setPitchShift(semitones) {
  state.pitchSemitones = semitones;
  const semitoneLabel = $('#pianoSemitoneLabel');
  if (semitoneLabel) {
    semitoneLabel.textContent = (semitones > 0 ? '+' : '') + semitones + ' ' + tr('player.semitones');
  }
  const badge = $('#pitchShiftBadge');
  if (badge) {
    badge.textContent = semitonesToDisplay(semitones);
    badge.classList.toggle('active', semitones !== 0);
  }
  const transBtn = $('#btnKeyTransposer');
  if (transBtn) {
    transBtn.classList.toggle('shifted', semitones !== 0);
  }
  const keyLabel = $('#playerKeyLabel');
  if (keyLabel) {
    keyLabel.textContent = calculateTransposedKey(state.sampleKey, semitones);
  }

  // Highlight active piano key
  $$('#pianoKeyboard .piano-key').forEach((k) => {
    const keySemi = parseInt(k.dataset.semitone, 10);
    const keyNote = k.dataset.note;
    const targetNote = semitoneToNote(semitones);
    k.classList.toggle('active', keySemi === semitones || keyNote === targetNote);
  });

  try {
    await bridge('audio.setPitchShift', { semitones });
  } catch (err) {
    console.error('setPitchShift failed:', err);
  }
}

async function resetOriginalKey() {
  state.pitchSemitones = 0;
  const semitoneLabel = $('#pianoSemitoneLabel');
  if (semitoneLabel) {
    semitoneLabel.textContent = '0 ' + tr('player.semitones');
  }
  const badge = $('#pitchShiftBadge');
  if (badge) {
    badge.textContent = '0st';
    badge.classList.remove('active');
  }
  const transBtn = $('#btnKeyTransposer');
  if (transBtn) {
    transBtn.classList.remove('shifted');
  }
  const keyLabel = $('#playerKeyLabel');
  if (keyLabel) {
    keyLabel.textContent = state.sampleKey || 'ORIGINAL';
  }
  $$('#pianoKeyboard .piano-key').forEach((k) => k.classList.remove('active'));

  try {
    await bridge('audio.setOriginalKey', {});
  } catch (err) {
    console.error('resetOriginalKey failed:', err);
  }
}

async function toggleSyncBpm() {
  state.syncBpm = !state.syncBpm;
  const btn = $('#btnSyncBpm');
  if (btn) btn.classList.toggle('on', state.syncBpm);

  try {
    if (state.syncBpm) {
      let dawTempo = 120.0;
      try {
        const tempoData = await bridge('reaper.tempo');
        if (tempoData && tempoData.bpm > 0) dawTempo = tempoData.bpm;
      } catch {}
      // If we don't have a sample BPM yet, try to detect it first
      let sampleBpm = state.sampleBpm;
      if (!sampleBpm || sampleBpm <= 0) {
        try {
          const path = state.selected || state.playingPath;
          if (path) {
            const det = await bridge('audio.detectBpm', { path });
            if (det && det.bpm > 0) {
              sampleBpm = det.bpm;
              state.sampleBpm = det.bpm;
            }
          }
        } catch {}
      }
      // Pass path as well so C++ can re-detect if still 0
      const curPath = state.selected || state.playingPath || '';
      await bridge('audio.setSyncBpm', { enabled: true, bpm: dawTempo, sampleBpm, path: curPath });
      if (!sampleBpm || sampleBpm <= 0) {
        toast(tr('sync.noBpm'));
      }
    } else {
      await bridge('audio.setSyncBpm', { enabled: false, ratio: 1.0 });
    }
  } catch (err) {
    console.error('toggleSyncBpm failed:', err);
  }
}

// ============ Tabs / Nav (sidebar per mockup v3) ============
const TABS = ['market', 'audioLab', 'agent', 'browser', 'account'];
const TAB_ICONS = {
  market: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M5 8h14l-1.2 12.2a1.8 1.8 0 0 1-1.8 1.6H8a1.8 1.8 0 0 1-1.8-1.6L5 8z"/><path d="M8.5 10.5V6.5a3.5 3.5 0 0 1 7 0v4"/></svg>',
  audioLab: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"><line x1="6" y1="4" x2="6" y2="20"/><line x1="12" y1="4" x2="12" y2="20"/><line x1="18" y1="4" x2="18" y2="20"/><circle cx="6" cy="9" r="2.2" fill="#101114"/><circle cx="12" cy="15" r="2.2" fill="#101114"/><circle cx="18" cy="8" r="2.2" fill="#101114"/></svg>',
  agent: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"><rect x="5" y="8" width="14" height="10" rx="2.5"/><line x1="12" y1="5" x2="12" y2="8"/><circle cx="12" cy="4.6" r="0.9" fill="currentColor" stroke="none"/><circle cx="9.5" cy="13" r="1" fill="currentColor" stroke="none"/><circle cx="14.5" cy="13" r="1" fill="currentColor" stroke="none"/></svg>',
  browser: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7"><circle cx="12" cy="12" r="8"/><ellipse cx="12" cy="12" rx="3.5" ry="8"/><line x1="4" y1="12" x2="20" y2="12"/></svg>',
  account: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"><circle cx="12" cy="8" r="3.5"/><path d="M5 20c1.4-3.2 4-4.8 7-4.8s5.6 1.6 7 4.8"/></svg>',
};
function renderNav() {
  const nav = $('#sidebar');
  if (!nav) return;
  nav.innerHTML = '';
  TABS.forEach((t) => {
    const b = document.createElement('button');
    b.className = 'nav-item' + (state.tab === t ? ' on' : '');
    b.innerHTML = TAB_ICONS[t] + '<span>' + tr('nav.' + t) + '</span>';
    b.onclick = () => { state.tab = t; renderNav(); showTab(t); };
    nav.appendChild(b);
  });
}
function showTab(t) {
  $$('.pane').forEach((p) => p.classList.remove('active'));
  $('#pane-' + t)?.classList.add('active');
}

function applyDockState(docked) {
  state.docked = !!docked;
  $('#app')?.classList.toggle('docked', state.docked);
  const btnDock = $('#btnDock');
  if (btnDock) {
    btnDock.textContent = state.docked ? '↗' : '📌';
    btnDock.title = state.docked ? tr('window.undock') : tr('window.dock');
    btnDock.classList.toggle('on', state.docked);
  }
}

// ============ Settings ============
async function initSettings() {
  try {
    const cfg = await bridge('config.getAll');
    if (cfg) {
      LANG = cfg.language || 'vi';
      applyAccent(cfg.accent || 'orange');
      applyNoise(cfg.noiseOverlay !== false);
      state.autoCollapseTree = cfg.autoCollapseTree !== false;
      applyDisplaySize(cfg.displaySize || 'medium');
      applyNavPosition(cfg.navPosition || 'top');
      if (cfg.treeWidth && $('#tree')) {
        $('#tree').style.width = cfg.treeWidth + 'px';
      }
      if (cfg.previewHeight && $('#preview')) {
        $('#preview').style.height = cfg.previewHeight + 'px';
      }
      state.scannerCpuMode = cfg.scannerCpuMode || 'normal';
      if (state.scannerCpuMode === 'extreme') state.scannerCpuMode = 'high';
      const scSelect = $('#scannerCpuMode');
      if (scSelect) {
        scSelect.value = state.scannerCpuMode;
        scSelect.onchange = () => {
          const val = scSelect.value;
          if (val === 'high') {
            if (!confirm(tr('scanner.cpuMode.highWarn'))) {
              scSelect.value = state.scannerCpuMode || 'normal';
              return;
            }
          }
          state.scannerCpuMode = val;
          bridge('scanner.setCpuMode', { cpuMode: val });
          bridge('config.set', { key: 'scannerCpuMode', value: val });
        };
      }
    }
  } catch (e) {
    console.warn('initSettings failed', e);
  }
  applyI18n();
  $('#btnSettings').onclick = (e) => {
    e.stopPropagation();
    const p = $('#settingsPop');
    if (p) {
      p.classList.toggle('hidden');
      if (!p.classList.contains('hidden') && p._render) p._render();
    }
  };
}
function applyDisplaySize(size) {
  state.displaySize = size || 'medium';
  document.body.classList.remove('size-small', 'size-medium', 'size-large');
  document.body.classList.add('size-' + state.displaySize);
  paintFromRaw(true);
}
function applyAccent(name) {
  const s = ACCENTS[name] || ACCENTS.orange;
  const st = document.documentElement.style;
  st.setProperty('--accent', s.accent);
  st.setProperty('--accent-hover', s.hover);
  st.setProperty('--accent-active', s.active);
  st.setProperty('--accent-soft', `rgba(${s.rgb},.12)`);
  st.setProperty('--accent-border', `rgba(${s.rgb},.35)`);
  st.setProperty('--accent-focus', `rgba(${s.rgb},.55)`);
  st.setProperty('--accent-glow', `rgba(${s.rgb},.08)`);
  state._accentCache = s.accent;
  drawWaveform();
  drawMeter();
}
function applyNoise(on) {
  $('#app')?.classList.toggle('no-noise', !on);
}
function applyNavPosition(pos) {
  state.navPosition = pos || 'top';
  const app = $('#app');
  const sidebar = $('#sidebar');
  const topSlot = $('#topNavSlot');
  const bodyRow = $('#bodyRow');
  const content = $('#content');
  if (!app || !sidebar || !topSlot || !bodyRow || !content) return;

  app.classList.remove('nav-top', 'nav-bottom', 'nav-left', 'nav-right');
  app.classList.add('nav-' + state.navPosition);

  if (state.navPosition === 'top') {
    topSlot.appendChild(sidebar);
  } else if (state.navPosition === 'bottom') {
    app.appendChild(sidebar);
  } else if (state.navPosition === 'right') {
    bodyRow.appendChild(sidebar);
  } else { // 'left'
    bodyRow.insertBefore(sidebar, content);
  }
  drawWaveform();
  drawMeter();
}
function buildSettingsPop() {
  let pop = $('#settingsPop');
  if (!pop) {
    pop = el('div');
    pop.id = 'settingsPop';
    pop.classList.add('hidden');
    document.body.appendChild(pop);
  }
  const renderPop = () => {
    pop.innerHTML = '';
    bridge('config.getAll').then((cfg) => {
      cfg = cfg || {};
      pop.appendChild(el('div', 'sp-title', tr('settings.displaySize')));
      [['small', 'size.small'], ['medium', 'size.medium'], ['large', 'size.large']].forEach(([v, k]) => {
        const o = el('div', 'sp-opt' + (state.displaySize === v ? ' on' : ''), tr(k));
        o.onclick = (e) => {
          e.stopPropagation();
          bridge('config.set', { key: 'displaySize', value: v });
          applyDisplaySize(v);
          renderPop();
        };
        pop.appendChild(o);
      });
      pop.appendChild(el('div', 'sp-title', tr('settings.navPosition')));
      [['top', 'pos.top'], ['bottom', 'pos.bottom'], ['left', 'pos.left'], ['right', 'pos.right']].forEach(([v, k]) => {
        const o = el('div', 'sp-opt' + ((state.navPosition || cfg.navPosition || 'top') === v ? ' on' : ''), tr(k));
        o.onclick = (e) => {
          e.stopPropagation();
          state.navPosition = v;
          bridge('config.set', { key: 'navPosition', value: v });
          applyNavPosition(v);
          renderPop();
        };
        pop.appendChild(o);
      });
      pop.appendChild(el('div', 'sp-title', tr('settings.browser')));
      const autoCol = cfg.autoCollapseTree !== false;
      const ac = el('div', 'sp-opt' + (autoCol ? ' on' : ''), (autoCol ? '✓ ' : '') + tr('settings.autoCollapse'));
      ac.onclick = (e) => {
        e.stopPropagation();
        const next = !autoCol;
        state.autoCollapseTree = next;
        bridge('config.set', { key: 'autoCollapseTree', value: next });
        if (next && state.currentDir) {
          tidyExpandedFolders(state.currentDir);
          renderTree();
        }
        renderPop();
      };
      pop.appendChild(ac);
      pop.appendChild(el('div', 'sp-title', tr('scanner.cpuMode')));
      const curCpu = state.scannerCpuMode || cfg.scannerCpuMode || 'normal';
      [['low', 'scanner.cpuMode.low'], ['normal', 'scanner.cpuMode.normal'], ['high', 'scanner.cpuMode.high']].forEach(([v, k]) => {
        const o = el('div', 'sp-opt' + (curCpu === v ? ' on' : ''), tr(k));
        o.onclick = (e) => {
          e.stopPropagation();
          if (v === 'high') {
            if (!confirm(tr('scanner.cpuMode.highWarn'))) return;
          }
          state.scannerCpuMode = v;
          bridge('scanner.setCpuMode', { cpuMode: v });
          bridge('config.set', { key: 'scannerCpuMode', value: v });
          const scSelect = $('#scannerCpuMode');
          if (scSelect) scSelect.value = v;
          renderPop();
        };
        pop.appendChild(o);
      });
      pop.appendChild(el('div', 'sp-title', tr('settings.accent')));
      [['orange', 'accent.orange'], ['amber', 'accent.amber'], ['muted', 'accent.muted'], ['gray', 'accent.gray']].forEach(([v, k]) => {
        const o = el('div', 'sp-opt' + (cfg.accent === v ? ' on' : ''), tr(k));
        o.onclick = (e) => { e.stopPropagation(); bridge('config.set', { key: 'accent', value: v }); applyAccent(v); renderPop(); };
        pop.appendChild(o);
      });
      pop.appendChild(el('div', 'sp-title', tr('settings.language')));
      const vi = el('div', 'sp-opt' + (cfg.language === 'vi' ? ' on' : ''), 'Tiếng Việt');
      vi.onclick = (e) => { e.stopPropagation(); LANG = 'vi'; bridge('config.set', { key: 'language', value: 'vi' }); applyI18n(); renderNav(); renderMarket(); renderPop(); };
      const en = el('div', 'sp-opt' + (cfg.language === 'en' ? ' on' : ''), 'English');
      en.onclick = (e) => { e.stopPropagation(); LANG = 'en'; bridge('config.set', { key: 'language', value: 'en' }); applyI18n(); renderNav(); renderMarket(); renderPop(); };
      pop.appendChild(vi); pop.appendChild(en);
      pop.appendChild(el('div', 'sp-title', tr('settings.effects')));
      const noiseOn = cfg.noiseOverlay !== false;
      const nz = el('div', 'sp-opt' + (noiseOn ? ' on' : ''), tr('settings.noise'));
      nz.onclick = (e) => {
        e.stopPropagation();
        const next = !noiseOn;
        bridge('config.set', { key: 'noiseOverlay', value: next });
        applyNoise(next);
        renderPop();
      };
      pop.appendChild(nz);
      pop.appendChild(el('div', 'sp-title', tr('settings.window')));
      const isDk = !!state.docked;
      const dockOpt = el('div', 'sp-opt' + (isDk ? ' on' : ''), (isDk ? '✓ ' : '') + tr('settings.dockToReaper'));
      dockOpt.onclick = (e) => {
        e.stopPropagation();
        bridge('window.toggleDock');
        pop.classList.add('hidden');
      };
      pop.appendChild(dockOpt);
    });
  };
  pop._render = renderPop;
  renderPop();
}

// ============ Browser ============
function getRowH() {
  if (state.displaySize === 'small') return 28;
  if (state.displaySize === 'large') return 44;
  return 34;
}
const VIRT_OVERSCAN = 8;

async function initBrowser() {
  state.roots = await bridge('fs.roots');
  if (state.roots.length) state.currentDir = state.roots[0].path;
  try {
    const t = await bridge('browser.tags');
    state.tagCache = (t && t.tags) || {};
  } catch { state.tagCache = {}; }
  renderRoots();
  renderTree();
  wireBrowserEvents();
  if (state.currentDir) openDir(state.currentDir);
}

async function refreshRoots() {
  state.roots = await bridge('fs.roots');
  renderRoots();
}

function renderRoots() {
  const sel = $('#roots');
  sel.innerHTML = '';
  state.roots.forEach((r) => {
    const o = el('option', '', r.name);
    o.value = r.path;
    sel.appendChild(o);
  });
  sel.value = state.currentDir || '';
  sel.onchange = () => {
    state.searchQ = '';
    $('#search').value = '';
    openDir(sel.value);
  };
}

async function subdirsOf(path) {
  if (state.subCache[path]) return state.subCache[path];
  const subs = await bridge('fs.subdirs', { path });
  state.subCache[path] = subs;
  return subs;
}

async function renderTree() {
  const tree = $('#tree');
  if (!tree) return;
  const seq = ++state.treeSeq;

  const favs = await bridge('browser.favorites');
  if (seq !== state.treeSeq) return;
  state.favSet = new Set(favs || []);

  const frag = document.createDocumentFragment();

  for (const root of state.roots) {
    frag.appendChild(folderRowEl(root.path, root.name, 0));
    if (state.expanded.has(root.path)) {
      const subs = await subdirsOf(root.path);
      if (seq !== state.treeSeq) return;
      for (const s of subs) {
        await drawSubFolder(frag, joinPath(root.path, s), s, 1, seq);
        if (seq !== state.treeSeq) return;
      }
    }
  }

  const currentScroll = tree.scrollTop;
  tree.replaceChildren(frag);
  tree.scrollTop = currentScroll;
}

async function drawSubFolder(parentEl, path, name, depth, seq) {
  if (seq && seq !== state.treeSeq) return;
  parentEl.appendChild(folderRowEl(path, name, depth));
  if (state.expanded.has(path)) {
    const subs = await subdirsOf(path);
    if (seq && seq !== state.treeSeq) return;
    for (const s of subs) {
      await drawSubFolder(parentEl, joinPath(path, s), s, depth + 1, seq);
      if (seq && seq !== state.treeSeq) return;
    }
  }
}

function tidyExpandedFolders(activePath) {
  if (!state.autoCollapseTree || !activePath) return;
  const sep = activePath.includes('\\') ? '\\' : '/';
  const normActive = (activePath.endsWith('\\') || activePath.endsWith('/'))
    ? activePath.slice(0, -1)
    : activePath;
  const needed = new Set();
  for (const exp of state.expanded) {
    const normExp = (exp.endsWith('\\') || exp.endsWith('/')) ? exp.slice(0, -1) : exp;
    if (normActive === normExp || normActive.startsWith(normExp + '\\') || normActive.startsWith(normExp + '/')) {
      needed.add(exp);
    }
  }
  needed.add(activePath);
  state.expanded = needed;
}

function folderRowEl(path, name, depth) {
  const row = el('div', 'tree-row' + (state.currentDir === path ? ' on' : ''));
  row.style.paddingLeft = 10 + depth * 12 + 'px';
  row.appendChild(el('span', 'twist', state.expanded.has(path) ? '▼' : '▶'));
  row.appendChild(el('span', 'name', name));
  row.onclick = (e) => {
    e.preventDefault();
    if (state.expanded.has(path)) {
      state.expanded.delete(path);
      const sep = path.includes('\\') ? '\\' : '/';
      const prefix = (path.endsWith('\\') || path.endsWith('/')) ? path : path + sep;
      for (const exp of Array.from(state.expanded)) {
        if (exp.startsWith(prefix)) state.expanded.delete(exp);
      }
    } else {
      if (state.autoCollapseTree) {
        tidyExpandedFolders(path);
      } else {
        state.expanded.add(path);
      }
    }
    if (state.currentDir !== path) openDir(path);
    renderTree();
  };
  row.oncontextmenu = (e) => { e.preventDefault(); folderMenu(e, path); };
  return row;
}

function sideFileRow(f) {
  const row = el('div', 'tree-row' + (state.selected === f.path ? ' on' : ''));
  row.appendChild(el('span', 'name', f.name));
  row.onclick = () => selectEntry(f);
  row.ondblclick = () => insertMedia(f.path);
  row.oncontextmenu = (e) => { e.preventDefault(); fileMenu(e, f); };
  armOleDrag(row, f);
  return row;
}

function joinPath(a, b) {
  const sep = a.includes('\\') ? '\\' : '/';
  return a.endsWith(sep) || a.endsWith('/') ? a + b : a + sep + b;
}

const DRAG_THRESH = 6;
function disarmOleDrag() {
  const a = state._dragArm;
  if (!a) return;
  window.removeEventListener('pointermove', a.move);
  window.removeEventListener('pointerup', a.up);
  window.removeEventListener('pointercancel', a.up);
  state._dragArm = null;
}
function armOleDrag(row, f) {
  if (!f || f.isDir || !f.path) return;
  row.draggable = false;
  row.addEventListener('pointerdown', (e) => {
    if (e.button !== 0) return;
    disarmOleDrag();
    const startX = e.clientX;
    const startY = e.clientY;
    const move = (ev) => {
      const dx = ev.clientX - startX;
      const dy = ev.clientY - startY;
      if (dx * dx + dy * dy < DRAG_THRESH * DRAG_THRESH) return;
      disarmOleDrag();
      state._suppressClick = true;
      setTimeout(() => { state._suppressClick = false; }, 500);
      if (hasWebView) {
        bridge('browser.beginDrag', {
          path: f.path,
          syncBpm: !!state.syncBpm,
          sampleBpm: f.bpm || (state.selected === f.path ? state.sampleBpm : 0) || 0,
          pitchSemitones: state.pitchSemitones || 0
        }).catch(() => {});
      }
    };
    const up = () => disarmOleDrag();
    state._dragArm = { move, up };
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
    window.addEventListener('pointercancel', up);
  });
}

function openDir(path) {
  const box = $('#files');
  if (box && state.currentDir && state.currentDir !== path) {
    state.dirScrolls[state.currentDir] = box.scrollTop;
  }
  state.currentDir = path;
  state.similarSource = null;
  state.similarSourceName = null;
  if (state.autoCollapseTree && path) {
    tidyExpandedFolders(path);
    renderTree();
  }
  state.searchQ = '';
  state.searchPending = false;
  state.searchGen = ++state.searchSeq;
  if ($('#search')) $('#search').value = '';
  const sel = $('#roots');
  if (sel) sel.value = path;
  bridge('fs.watch', { path }).catch(() => {});
  loadDir(path, false);
}

function loadDir(path, force) {
  if (!path) return;
  const reqId = ++state.listSeq;
  const go = () => bridge('fs.list', { path, sort: state.sort }).then((files) => {
    if (reqId !== state.listSeq) return;
    state.rawFiles = files || [];
    const isReloadingCurrent = (state.listDir === path);
    state.listDir = path;
    paintFromRaw(isReloadingCurrent);
    probeVisibleAudio();
  });
  if (force) bridge('fs.invalidate', { path }).then(go, go);
  else go();
}

function filteredFiles() {
  return state.rawFiles.filter((f) => {
    if (f.isDir) return false;
    if (f.name && (f.name.toLowerCase() === 'desktop.ini' || f.name.toLowerCase() === 'thumbs.db' || f.name === '.DS_Store')) return false;
    if (state.audioOnly && !f.isAudio) return false;
    if (state.favOnly && !state.favSet.has(f.path)) return false;
    if (state.tagFilter > 0 && (state.tagCache[f.path] || 0) !== state.tagFilter) return false;
    return true;
  });
}

function paintFromRaw(preserveScroll = false) {
  if (state.similarSource) {
    state.files = state.rawFiles || [];
  } else {
    state.files = filteredFiles();
  }
  const box = $('#files');
  if (!box) return;

  const savedScroll = preserveScroll ? box.scrollTop : (state.searchQ ? 0 : (state.dirScrolls[state.currentDir] || 0));

  let header = box.querySelector('.files-head');
  if (!header) {
    header = el('div', 'files-head');
    box.appendChild(header);
  }
  const q = (state.searchQ || '').trim();
  if (state.similarSource) {
    header.textContent = `${tr('browser.similarTo')}: ${state.similarSourceName || ''} (${state.files.length})`;
  } else {
    header.textContent = q
      ? (state.searchPending ? tr('browser.searching') : `${tr('browser.results')}: ${state.files.length}`)
      : (state.currentDir || tr('browser.pickRoot'));
  }

  paintSimilarBanner();

  let spacer = $('#fileSpacer');
  if (!spacer) {
    spacer = el('div');
    spacer.id = 'fileSpacer';
    box.appendChild(spacer);
  }

  const rowH = getRowH();
  spacer.style.height = Math.max(rowH, state.files.length * rowH) + 'px';
  box.scrollTop = savedScroll;
  paintVisible();
}

function paintVisible() {
  const spacer = $('#fileSpacer');
  const box = $('#files');
  if (!spacer || !box) return;
  const files = state.files;
  const headerH = 24;
  const total = files.length;
  const rowH = getRowH();
  spacer.style.height = Math.max(rowH, total * rowH) + 'px';
  const scroll = box.scrollTop - headerH;
  const viewH = box.clientHeight || 300;
  let start = Math.max(0, Math.floor(scroll / rowH) - VIRT_OVERSCAN);
  let end = Math.min(total, Math.ceil((scroll + viewH) / rowH) + VIRT_OVERSCAN);
  if (total <= 80) { start = 0; end = total; }
  spacer.replaceChildren();
  for (let i = start; i < end; ++i) {
    const row = fileRowEl(files[i], state.selected === files[i].path, false);
    row.style.position = 'absolute';
    row.style.left = '0';
    row.style.right = '0';
    row.style.top = (i * rowH) + 'px';
    spacer.appendChild(row);
  }
  if (!total) {
    const empty = el('div', 'tree-row muted', tr('browser.empty'));
    empty.style.position = 'absolute';
    empty.style.top = '0';
    spacer.appendChild(empty);
  }
}

function fileRowEl(f, isSelected, compact) {
  const row = el('div', 'file-row' + (isSelected ? ' sel' : '') + (f.isDir ? ' dir' : ''));
  row._path = f.path;
  const tag = (state.tagCache && state.tagCache[f.path]) || 0;
  if (tag > 0) {
    const d = el('span', 'tagdot');
    d.style.background = TAG_COLORS[tag];
    row.appendChild(d);
  } else {
    row.appendChild(el('span', 'tagdot'));
  }
  const label = f.isDir ? '▸ ' + f.name : f.name;
  row.appendChild(el('span', 'fname', label));
  if (!compact && !f.isDir) {
    if (f.duration && !state.probeCache[f.path]) state.probeCache[f.path] = f.duration;
    const dur = state.probeCache[f.path];
    if (dur) row.appendChild(el('span', 'fdur', fmtDur(dur)));
    if (f.similarity !== undefined || (f.score !== undefined && f.score > 0 && f.score <= 1.0)) {
      const matchPct = f.similarity || Math.round(f.score * 100);
      if (matchPct > 0) {
        row.appendChild(el('span', 'fmeta-badge sim-badge', matchPct + '% ' + tr('browser.matchPercent')));
      }
    }
    if (f.bpm && f.bpm > 0) {
      row.appendChild(el('span', 'fmeta-badge', Math.round(f.bpm) + ' BPM'));
    }
    if (f.key) {
      row.appendChild(el('span', 'fmeta-badge', f.key));
    }
    if (f.genre) {
      row.appendChild(el('span', 'fmeta-badge genre', f.genre));
    }
    if (f.size !== undefined) row.appendChild(el('span', 'fsize', fmtSize(f.size)));
  }
  if (state.favSet && state.favSet.has(f.path)) row.appendChild(el('span', 'star', '★'));
  row.onclick = (e) => {
    if (state._suppressClick) { e.preventDefault(); e.stopPropagation(); return; }
    e.preventDefault();
    selectEntry(f);
  };
  row.ondblclick = () => {
    if (f.isDir) openDir(f.path);
    else insertMedia(f.path);
  };
  row.oncontextmenu = (e) => { e.preventDefault(); fileMenu(e, f); };
  armOleDrag(row, f);
  return row;
}

function selectEntry(f) {
  state.selected = f.path;
  const spacer = $('#fileSpacer');
  if (spacer) {
    spacer.querySelectorAll('.file-row').forEach((r) => {
      r.classList.toggle('sel', r._path === f.path);
    });
  }
  if (!f.isDir && f.isAudio) {
    // Defer auto-preview by one frame so a dblclick (insert) has a chance to
    // fire first — otherwise the user double-clicking to insert also pays the
    // cost of decoding+stopping an audio preview.
    if (!state.autoPreview) return;
    clearTimeout(state._previewArmT);
    state._previewArmT = setTimeout(() => {
      // Only fire if selection hasn't moved on.
      if (state.selected === f.path) playFile(f.path);
    }, 120);
  }
}

let _probeBatchTimer = null;
let _scrollProbeTimer = null;
function probeVisibleAudio() {
  const box = $('#files');
  const files = state.files;
  if (!box || !files || !files.length) return;
  const headerH = 24;
  const scroll = Math.max(0, box.scrollTop - headerH);
  const viewH = box.clientHeight || 300;
  const total = files.length;
  const rowH = getRowH();
  let start = Math.max(0, Math.floor(scroll / rowH) - 2);
  let end = Math.min(total, Math.ceil((scroll + viewH) / rowH) + 2);
  if (total <= 40) { start = 0; end = total; }

  const slice = files.slice(start, end).filter((f) => f.isAudio && !state.probeCache[f.path] && !state.probeInflight.has(f.path));
  slice.slice(0, 24).forEach((f) => {
    state.probeInflight.add(f.path);
    bridge('audio.probe', { path: f.path }).then((d) => {
      state.probeInflight.delete(f.path);
      if (d && d.ok && d.duration) {
        state.probeCache[f.path] = d.duration;
        if (!_probeBatchTimer) {
          _probeBatchTimer = setTimeout(() => {
            _probeBatchTimer = null;
            paintVisible();
          }, 40);
        }
      }
    }).catch(() => { state.probeInflight.delete(f.path); });
  });
}

function runSearch(q) {
  if (!q) return;
  const gen = ++state.searchSeq;
  state.searchGen = gen;
  state.searchPending = true;
  state.similarSource = null;
  state.similarSourceName = null;
  state.rawFiles = [];
  paintFromRaw(false);
  bridge('browser.search', { base: state.currentDir || '', query: q, audioOnly: state.audioOnly, gen })
    .catch(() => {
      if (state.searchGen === gen) {
        state.searchPending = false;
        paintFromRaw(false);
      }
    });
}

function findSimilarSamples(f) {
  if (!f || !f.path) return;
  const targetName = f.name || f.path.split(/[\\/]/).pop();
  toast(tr('browser.ctx.findSimilar') + '...');
  state.similarSource = f.path;
  state.similarSourceName = targetName;
  state.searchQ = '';
  if ($('#search')) $('#search').value = '';

  bridge('search.findSimilar', { path: f.path, limit: 100 }).then((res) => {
    if (res && res.results && res.results.length > 0) {
      state.rawFiles = res.results.map((r) => ({
        path: r.path,
        name: r.filename || r.path.split(/[\\/]/).pop(),
        isDir: false,
        isAudio: true,
        duration: r.duration,
        bpm: r.bpm,
        key: r.key ? (r.key + (r.mode === 'minor' ? 'm' : '')) : (r.camelot || ''),
        genre: r.genre || r.mood || '',
        score: r.score,
        similarity: r.similarity || Math.round((r.score || 0) * 100),
        size: r.filesize,
      }));
      state.files = state.rawFiles;
      paintFromRaw(false);
    } else {
      toast(tr('browser.noResults') || 'No similar samples found');
    }
  }).catch((err) => {
    console.error('findSimilar error', err);
    toast(tr('toast.similarError'));
  });
}

function clearSimilarFilter() {
  state.similarSource = null;
  state.similarSourceName = null;
  const banner = $('#similarBanner');
  if (banner) banner.classList.add('hidden');
  if (state.currentDir) loadDir(state.currentDir, false);
}

function paintSimilarBanner() {
  const box = $('#files');
  if (!box) return;
  let banner = $('#similarBanner');
  if (!banner) {
    banner = el('div', 'similar-banner');
    banner.id = 'similarBanner';
    box.prepend(banner);
  }
  if (!state.similarSource) {
    banner.classList.add('hidden');
    return;
  }
  banner.classList.remove('hidden');
  banner.replaceChildren();

  const info = el('div', 'similar-banner-info');
  info.appendChild(el('span', 'similar-banner-icon', '🔍'));
  info.appendChild(el('span', 'similar-banner-label', tr('browser.similarTo') + ':'));
  info.appendChild(el('span', 'similar-target-name', state.similarSourceName || ''));
  info.appendChild(el('span', 'similar-count-badge', `${state.files.length} ${tr('browser.matchPercent')}`));
  banner.appendChild(info);

  const btnClose = el('button', 'btn-exit-similar', '✕');
  btnClose.title = tr('browser.clearSimilar');
  btnClose.onclick = () => clearSimilarFilter();
  banner.appendChild(btnClose);
}

function selectedIndex() {
  return state.files.findIndex((f) => f.path === state.selected);
}

function moveSelection(delta) {
  if (!state.files.length) return;
  let i = selectedIndex();
  if (i < 0) i = 0;
  else i = Math.max(0, Math.min(state.files.length - 1, i + delta));
  const f = state.files[i];
  state.selected = f.path;
  const box = $('#files');
  if (box) {
    const rowH = getRowH();
    const rowTop = i * rowH;
    const rowBottom = rowTop + rowH;
    const viewTop = box.scrollTop;
    const viewBottom = box.scrollTop + (box.clientHeight || 300);
    if (rowTop < viewTop) {
      box.scrollTop = rowTop;
    } else if (rowBottom > viewBottom) {
      box.scrollTop = rowBottom - (box.clientHeight || 300);
    }
  }
  paintVisible();
  if (state.autoPreview && f.isAudio) playFile(f.path);
}

function wireBrowserEvents() {
  let searchTimer = null;
  const searchInput = $('#search');
  const searchPop = $('#searchSuggest');

  const updateSuggestions = (val) => {
    if (!searchPop) return;
    const text = val || '';
    const lastWord = text.split(/\s+/).pop() || '';
    if (!lastWord.startsWith('/')) {
      searchPop.classList.add('hidden');
      return;
    }
    bridge('browser.suggestTags', { query: lastWord }).then((data) => {
      const list = (data && data.suggestions) ? data.suggestions : [
        '/bpm:120-130', '/bpm:140', '/key:Am', '/key:F#m', '/fav',
        '/trap', '/lo-fi', '/hiphop', '/house', '/drill', '/ambient',
        '/kick', '/snare', '/hihat', '/808', '/vocal', '/bass', '/synth'
      ];
      searchPop.replaceChildren();
      if (!list.length) { searchPop.classList.add('hidden'); return; }
      list.forEach((tag) => {
        const chip = el('div', 'suggest-chip', tag);
        chip.onmousedown = (ev) => {
          ev.preventDefault();
          const words = text.split(/\s+/);
          words.pop();
          words.push(tag);
          if (searchInput) {
            searchInput.value = words.join(' ') + ' ';
            state.searchQ = searchInput.value;
            searchPop.classList.add('hidden');
            runSearch(state.searchQ.trim());
          }
        };
        searchPop.appendChild(chip);
      });
      searchPop.classList.remove('hidden');
    }).catch(() => { searchPop.classList.add('hidden'); });
  };

  if (searchInput) {
    searchInput.addEventListener('input', (e) => {
      clearTimeout(searchTimer);
      const val = e.target.value;
      updateSuggestions(val);
      searchTimer = setTimeout(() => {
        state.searchQ = val;
        const q = (state.searchQ || '').trim();
        if (q) runSearch(q);
        else if (state.currentDir) {
          state.searchPending = false;
          state.searchGen = ++state.searchSeq;
          loadDir(state.currentDir, false);
        }
      }, 200);
    });
    searchInput.addEventListener('focus', (e) => {
      updateSuggestions(e.target.value);
    });
    searchInput.addEventListener('blur', () => {
      setTimeout(() => { if (searchPop) searchPop.classList.add('hidden'); }, 200);
    });
    searchInput.addEventListener('keydown', (e) => {
      if (e.key === 'Escape' && searchPop) {
        searchPop.classList.add('hidden');
      }
    });
  }

  $('#sort').onchange = (e) => {
    state.sort = +e.target.value;
    if (state.currentDir && !(state.searchQ || '').trim()) loadDir(state.currentDir, true);
    else paintFromRaw(true);
  };
  $('#audioOnly').onclick = (e) => {
    state.audioOnly = !state.audioOnly;
    e.target.classList.toggle('on', state.audioOnly);
    paintFromRaw(true);
  };
  const autoBtn = $('#autoPreview');
  if (autoBtn) {
    autoBtn.classList.toggle('on', state.autoPreview);
    autoBtn.onclick = (e) => {
      state.autoPreview = !state.autoPreview;
      e.target.classList.toggle('on', state.autoPreview);
    };
  }
  const favBtn = $('#favOnly');
  if (favBtn) {
    favBtn.title = tr('browser.favOnly');
    favBtn.onclick = (e) => {
      state.favOnly = !state.favOnly;
      e.target.classList.toggle('on', state.favOnly);
      paintFromRaw(true);
    };
  }
  const tagSel = $('#tagFilter');
  if (tagSel) tagSel.onchange = (e) => { state.tagFilter = +e.target.value; paintFromRaw(true); };
  $('#btnRefresh').onclick = () => {
    state.subCache = {};
    if (state.currentDir) loadDir(state.currentDir, true);
    renderTree();
  };
  const cancelScanBtn = $('#btnScannerCancel');
  if (cancelScanBtn) {
    cancelScanBtn.onclick = () => {
      bridge('scanner.cancel').then(() => {
        toast(tr('scanner.cancelled'));
        const bar = $('#scannerBar');
        if (bar) bar.classList.add('hidden');
      });
    };
  }
  $('#files').addEventListener('scroll', () => {
    paintVisible();
    clearTimeout(_scrollProbeTimer);
    _scrollProbeTimer = setTimeout(probeVisibleAudio, 100);
  });

  $('#btnPlay').onclick = () => {
    if (state.playing) bridge('audio.stop').then(refreshPlayState);
    else if (state.selected) playFile(state.selected);
  };
  $('#btnLoop').onclick = (e) => {
    state.loop = !state.loop;
    e.target.classList.toggle('on', state.loop);
    bridge('audio.setLoop', { value: state.loop });
  };
  const syncBpmBtn = $('#btnSyncBpm');
  if (syncBpmBtn) {
    syncBpmBtn.onclick = () => toggleSyncBpm();
  }
  const keyTransBtn = $('#btnKeyTransposer');
  if (keyTransBtn) {
    keyTransBtn.onclick = (e) => {
      e.stopPropagation();
      $('#pianoTransposerPop')?.classList.toggle('hidden');
    };
  }
  $$('#pianoKeyboard .piano-key').forEach((k) => {
    k.onclick = (e) => {
      e.stopPropagation();
      const semitone = parseInt(k.dataset.semitone, 10);
      setPitchShift(semitone);
    };
  });
  const resetKeyBtn = $('#btnResetKey');
  if (resetKeyBtn) {
    resetKeyBtn.onclick = (e) => {
      e.stopPropagation();
      resetOriginalKey();
    };
  }
  $('#btnLab').onclick = () => { if (state.selected) insertMediaLab(state.selected); };
  $('#volume').oninput = (e) => bridge('audio.setVolume', { value: +e.target.value });
  $('#waveform').addEventListener('click', (e) => {
    if (!state.duration) return;
    const r = e.currentTarget.getBoundingClientRect();
    const frac = Math.max(0, Math.min(1, (e.clientX - r.left) / r.width));
    bridge('audio.seek', { fraction: frac });
    state.position = frac;
    drawWaveform();
  });

  document.addEventListener('click', (e) => {
    if (!e.target.closest('#ctxMenu')) $('#ctxMenu')?.classList.add('hidden');
    if (!e.target.closest('#settingsPop') && !e.target.closest('#btnSettings')) $('#settingsPop')?.classList.add('hidden');
    if (!e.target.closest('#pianoTransposerPop') && !e.target.closest('#btnKeyTransposer')) $('#pianoTransposerPop')?.classList.add('hidden');
  });
  document.addEventListener('keydown', onBrowserKey);
  window.addEventListener('resize', () => { drawWaveform(); paintVisible(); });
}

// Expose stub-note visibility for production builds. Stubs are dev-only —
// in real WebView2 they leak "Phase X — TODO" noise to end users.
function applyStubVisibility() {
  const isDev = !hasWebView || new URLSearchParams(location.search).has('dev');
  document.body.classList.toggle('show-stubs', isDev);
}

function typingInField() {
  const t = document.activeElement;
  if (!t) return false;
  const tag = (t.tagName || '').toLowerCase();
  return tag === 'input' || tag === 'textarea' || tag === 'select' || t.isContentEditable;
}

function onBrowserKey(e) {
  if (e.key === 'F5' || (e.ctrlKey && (e.key === 'r' || e.key === 'R'))) {
    e.preventDefault();
    window.location.reload();
    return;
  }
  // Ctrl+K focuses the browser search bar from anywhere.
  if (e.ctrlKey && (e.key === 'k' || e.key === 'K')) {
    e.preventDefault();
    if (state.tab !== 'browser') {
      state.tab = 'browser';
      renderNav();
      showTab('browser');
    }
    const s = $('#search');
    if (s) { s.focus(); s.select(); }
    return;
  }
  if (typingInField()) {
    if (e.key === 'Escape') { e.target.blur(); e.preventDefault(); }
    return;
  }
  if (e.key === ' ') {
    e.preventDefault();
    if (state.playing) {
      state.playing = false;
      const bp = $('#btnPlay');
      if (bp) bp.textContent = '>';
    }
    bridge('reaper.playToggle').then(refreshPlayState);
    return;
  }
  if (state.tab !== 'browser') return;
  if (e.key === 'ArrowDown') { e.preventDefault(); moveSelection(1); }
  else if (e.key === 'ArrowUp') { e.preventDefault(); moveSelection(-1); }
  else if (e.key === 'Enter') {
    e.preventDefault();
    const f = state.files.find((x) => x.path === state.selected);
    if (!f) return;
    if (f.isDir) openDir(f.path);
    else insertMedia(f.path);
  } else if (e.key === 'Backspace') {
    e.preventDefault();
    if (!state.currentDir) return;
    if (state.roots.some((r) => r.path === state.currentDir)) return;
    openDir(parentDir(state.currentDir));
  } else if (e.key === 'Escape') {
    $('#ctxMenu')?.classList.add('hidden');
    $('#pianoTransposerPop')?.classList.add('hidden');
    bridge('audio.stop').then(refreshPlayState);
  } else if (e.key === 'F2') {
    const f = state.files.find((x) => x.path === state.selected);
    if (f && !f.isDir) startRename(f);
  } else if (e.key === 'Delete') {
    const f = state.files.find((x) => x.path === state.selected);
    if (f && !f.isDir) startDelete(f);
  }
}

// ============ Preview / audio ============
// Stop any currently-playing file before starting a new one — without this,
// auto-preview clicking fast through the file list can trigger overlapping
// decoders in the C++ audio backend.
async function playFile(path) {
  try {
    if (state.playing) {
      try { await bridge('audio.stop'); } catch {}
      state.playing = false;
    }
    const fileObj = (state.files || []).find((x) => x.path === path);
    const sampleBpm = (fileObj && fileObj.bpm) || (state.selected === path ? state.sampleBpm : 0) || 0;
    const d = await bridge('audio.play', {
      path,
      loop: state.loop,
      syncBpm: !!state.syncBpm,
      sampleBpm: sampleBpm
    });
    if (!d || d.ok === false) { toast(tr('toast.decodeFail')); return; }
    state.playingPath = path;
    state.selected = path;
    state.envelope = d.envelope || [];
    state.duration = d.duration || 0;
    state.playing = true;
    if (d.duration) state.probeCache[path] = d.duration;
    const bp = $('#btnPlay');
    if (bp) bp.textContent = 'II';

    const filename = path.split(/[\\/]/).pop() || '';
    const info = $('#trackInfo');
    if (info) info.textContent = `${filename} | ${d.sampleRate}Hz ${d.channels}ch`;

    // Dynamic player tags extraction
    const tags = extractTagsFromFilename(filename);
    state.sampleTags = tags;
    renderPlayerTags(tags);

    // BPM & Key extraction: try DB via getSampleMeta first, fallback to filename regex
    const keyLabel = $('#playerKeyLabel');
    // Default from filename
    const bpmMatch = filename.match(/(\d+)\s*bpm/i);
    const filenameBpm = bpmMatch ? parseFloat(bpmMatch[1]) : 0;
    const keyMatch = filename.match(/_([A-G][#b]?(?:m|maj|min|minor|major)?)(?:_|\.|$)/i);
    const filenameKey = keyMatch ? keyMatch[1].toUpperCase() : 'ORIGINAL';

    // Try to get real metadata from DB / detection
    bridge('audio.getSampleMeta', { path }).then((meta) => {
      let bpm = 0, key = 'ORIGINAL';
      let genre = '', mood = '';
      if (meta && meta.ok !== false) {
        if (meta.bpm && meta.bpm > 30) bpm = meta.bpm;
        if (meta.key) key = meta.key;
        if (meta.genre) genre = meta.genre;
        if (meta.mood) mood = meta.mood;
      }
      // Fallback to filename if DB has nothing
      if (!bpm) bpm = filenameBpm;
      if (key === 'ORIGINAL' || !key) key = filenameKey;

      state.sampleBpm = bpm || 0;
      state.sampleKey = key || 'ORIGINAL';

      // Update key label
      if (keyLabel) keyLabel.textContent = calculateTransposedKey(state.sampleKey, state.pitchSemitones);

      // Render tags from DB if available, else heuristic
      if (genre || mood) {
        const tags = [];
        if (genre) tags.push({ name: genre, type: 'genre' });
        if (mood) tags.push({ name: mood, type: 'mood' });
        // Also add key as tag
        if (key && key !== 'ORIGINAL') tags.push({ name: key, type: 'prop' });
        state.sampleTags = tags;
        renderPlayerTags(tags);
      }

      // If Sync is already on and we just discovered BPM, re-apply sync
      if (state.syncBpm && bpm) {
        bridge('reaper.tempo').then((td) => {
          const dawBpm = (td && td.bpm) ? td.bpm : 120;
          bridge('audio.setSyncBpm', { enabled: true, bpm: dawBpm, sampleBpm: bpm, path }).catch(()=>{});
        }).catch(()=>{});
      }
    }).catch(() => {
      // Fallback to filename only
      state.sampleBpm = filenameBpm;
      state.sampleKey = filenameKey;
      if (keyLabel) keyLabel.textContent = calculateTransposedKey(state.sampleKey, state.pitchSemitones);
    });

    if (keyLabel && !keyLabel.textContent) keyLabel.textContent = calculateTransposedKey(state.sampleKey, state.pitchSemitones);

    drawWaveform();
  } catch {
    toast(tr('toast.decodeFail'));
  }
}
function refreshPlayState() {
  state.playing = false;
  $('#btnPlay').textContent = '>';
  drawWaveform();
}
function updatePreviewLive() {
  $('#timeLabel').textContent = `${(state.position * state.duration).toFixed(1)} / ${state.duration.toFixed(1)}s`;
  drawMeter();
  drawWaveform();
}
function drawWaveform() {
  const c = $('#waveform');
  if (!c) return;
  const w = c.clientWidth || 300;
  const h = c.clientHeight || 44;
  if (c.width !== w) c.width = w;
  if (c.height !== h) c.height = h;
  const ctx = c.getContext('2d');
  const W = c.width, H = c.height;
  ctx.clearRect(0, 0, W, H);
  const acc = state._accentCache || '#FF6B2C';
  const env = state.envelope;
  const mid = H / 2;
  const amp = H / 2 - 2;
  const px = (W - 2) * Math.min(1, Math.max(0, state.position));

  if (env && env.length) {
    const numBars = env.length;
    const barW = W / numBars;
    const drawW = Math.max(1, barW - 0.7);

    // Render waveform bars with played vs unplayed two-tone coloring
    for (let i = 0; i < numBars; ++i) {
      const x = i * barW;
      const val = env[i];
      const curved = Math.pow(Math.min(1, val), 0.75);
      const h = Math.max(0.75, curved * amp);
      const isPlayed = (x + barW / 2) <= px;

      ctx.fillStyle = isPlayed ? acc : 'rgba(255,255,255,0.22)';
      ctx.fillRect(x, mid - h, drawW, Math.max(1.5, h * 2));
    }

    // Playhead cursor
    if (state.playing || state.position > 0) {
      ctx.fillStyle = '#FFFFFF';
      ctx.fillRect(px, 0, 1.5, H);
    }
  } else {
    // Centerline when idle / loading
    ctx.fillStyle = 'rgba(255,255,255,0.12)';
    ctx.fillRect(0, mid - 0.5, W, 1);
    if (state.playing || state.position > 0) {
      ctx.fillStyle = '#FFFFFF';
      ctx.fillRect(px, 0, 1.5, H);
    }
  }
}
function drawMeter() {
  const c = $('#meter');
  const w = c.clientWidth || 300;
  if (c.width !== w) c.width = w;
  const ctx = c.getContext('2d');
  const W = c.width, H = c.height;
  ctx.clearRect(0, 0, W, H);
  const fillW = (W - 4) * Math.min(1, state.peak);
  ctx.fillStyle = state._accentCache || '#FF6B2C';
  if (fillW > 0.5) ctx.fillRect(2, 2, fillW, H - 4);
}

// ============ Insert / Lab ============
function insertMedia(path) {
  if (!path) return;
  const name = path.split(/[\\/]/).pop() || '';
  const ext = (name.split('.').pop() || '').toLowerCase();
  const media = 'wav,wave,mp3,flac,ogg,oga,aif,aiff,wma,m4a,aac,opus,w64,caf,mid,midi,mp4,mkv,mov,avi,webm,wmv,rpp,rtracktemplate,rfxchain'.split(',');
  if (!media.includes(ext)) { toast(tr('toast.notMedia')); return; }
  const fileObj = (state.files || []).find((f) => f.path === path);
  const args = {
    path,
    syncBpm: !!state.syncBpm,
    sampleBpm: (fileObj && fileObj.bpm) || (state.selected === path ? state.sampleBpm : 0) || 0,
    pitchSemitones: state.pitchSemitones || 0
  };
  bridge('reaper.insert', args).then((d) => {
    if (d && d.synced) toast(tr('toast.inserted') + ' (Sync ' + (d.playrate ? d.playrate.toFixed(2) + 'x' : '') + ')');
    else toast(tr('toast.inserted'));
  });
}
function insertMediaLab(path) { bridge('reaper.lab', { path, job: 'analyze' }).then(() => toast(tr('toast.labQueued'))); }

function startRename(f) {
  if (!f || f.isDir) return;
  $('#renameModal').classList.remove('hidden');
  $('#renameInput').value = f.name;
  $('#renameInput').focus();
  $('#renameOk').onclick = () => {
    const newName = ($('#renameInput').value || '').trim();
    if (!newName || newName === f.name) { $('#renameModal').classList.add('hidden'); return; }
    const sep = f.path.includes('\\') ? '\\' : '/';
    const dir = f.path.slice(0, f.path.lastIndexOf(sep));
    bridge('browser.rename', { from: f.path, to: joinPath(dir, newName) })
      .then(() => {
        $('#renameModal').classList.add('hidden');
        toast(tr('toast.renamed'));
        if (state.tagCache && state.tagCache[f.path]) {
          state.tagCache[joinPath(dir, newName)] = state.tagCache[f.path];
          delete state.tagCache[f.path];
        }
        if (state.favSet.has(f.path)) {
          state.favSet.delete(f.path);
          state.favSet.add(joinPath(dir, newName));
        }
        if (state.selected === f.path) state.selected = joinPath(dir, newName);
        loadDir(dir, true);
        renderTree();
      })
      .catch(() => { $('#renameModal').classList.add('hidden'); toast(tr('toast.renameFail')); });
  };
  $('#renameCancel').onclick = () => $('#renameModal').classList.add('hidden');
}

function startDelete(f) {
  if (!f || f.isDir) return;
  $('#deleteModal').classList.remove('hidden');
  $('#deletePath').textContent = f.path;
  $('#deleteOk').onclick = () => {
    bridge('browser.delete', { path: f.path })
      .then(() => {
        $('#deleteModal').classList.add('hidden');
        toast(tr('toast.deleted'));
        if (state.tagCache) delete state.tagCache[f.path];
        state.favSet.delete(f.path);
        if (state.selected === f.path) state.selected = null;
        const sep = f.path.includes('\\') ? '\\' : '/';
        const dir = f.path.slice(0, f.path.lastIndexOf(sep));
        loadDir(dir, true);
        renderTree();
      })
      .catch(() => { $('#deleteModal').classList.add('hidden'); toast(tr('toast.deleteFail')); });
  };
  $('#deleteCancel').onclick = () => $('#deleteModal').classList.add('hidden');
}

// ============ Context menus ============
function showMenu(e, items) {
  const m = $('#ctxMenu');
  m.innerHTML = '';
  items.forEach((it) => {
    if (it === '-') { m.appendChild(el('div', 'ctx-sep')); return; }
    const row = el('div', 'ctx-item');
    row.appendChild(el('span', '', it.label));
    if (it.sub) row.appendChild(el('span', 'sub', it.sub));
    row.onclick = () => { m.classList.add('hidden'); it.action(); };
    m.appendChild(row);
  });
  m.classList.remove('hidden');
  const mw = 250, mh = m.offsetHeight || 200;
  m.style.left = Math.min(e.clientX, window.innerWidth - mw - 8) + 'px';
  m.style.top = Math.min(e.clientY, window.innerHeight - mh - 8) + 'px';
}
function triggerFolderScan(folderPath, forceRescan = false) {
  if (!folderPath) return;
  const bar = $('#scannerBar');
  const status = $('#scannerStatusText');
  const file = $('#scannerFileText');
  const fill = $('#scannerProgressFill');
  const scSelect = $('#scannerCpuMode');
  if (scSelect) scSelect.value = state.scannerCpuMode || 'normal';
  if (bar) bar.classList.remove('hidden');
  if (status) status.textContent = tr('scanner.starting');
  if (file) file.textContent = folderPath;
  if (fill) fill.style.width = '5%';
  toast(tr('scanner.starting'));
  bridge('scanner.start', {
    path: folderPath,
    forceRescan: !!forceRescan,
    cpuMode: state.scannerCpuMode || 'normal'
  }).catch((err) => {
    toast(tr('toast.scannerError') + (err ? ': ' + err : ''));
    if (bar) bar.classList.add('hidden');
  });
}
// Route a context-menu lab job to the Audio Lab tab and start it there, so
// progress/result UI is visible (instead of the old P2 toast stub).
const LAB_MENU_TO_TOOL = { stem: 'stem', denoise: 'denoise', keychord: 'keychord', tempo: 'analyze', midi: 'keychord' };
function sendToLabJob(path, job) {
  setLabFile(path);
  state.tab = 'audioLab';
  renderNav();
  showTab('audioLab');
  const tool = LAB_MENU_TO_TOOL[job] || job;
  setTimeout(() => {
    const b = document.querySelector(`#labGrid .lab-tool[data-job="${tool}"]`);
    if (b) b.click();
  }, 0);
}

function fileMenu(e, f) {
  state.selected = f.path;
  paintVisible();
  const items = [];
  if (f.isDir) {
    items.push({ label: tr('browser.ctx.scanNew'), action: () => triggerFolderScan(f.path, false) });
    items.push({ label: tr('browser.ctx.rescanAll'), action: () => triggerFolderScan(f.path, true) });
    items.push({ label: tr('browser.ctx.openHere'), action: () => openDir(f.path) });
    items.push('-');
    items.push({ label: tr('browser.ctx.copyPath'), action: () => { navigator.clipboard?.writeText(f.path); toast(tr('toast.copied')); } });
    items.push({ label: tr('browser.ctx.reveal'), action: () => bridge('reaper.reveal', { path: f.path }) });
    showMenu(e, items);
    return;
  }
  if (f.isAudio) {
    items.push({ label: tr('browser.ctx.preview'), action: () => playFile(f.path) });
    items.push({ label: tr('browser.ctx.insert'), sub: 'Enter', action: () => insertMedia(f.path) });
    items.push({ label: tr('browser.ctx.findSimilar'), sub: 'AI', action: () => findSimilarSamples(f) });
    items.push('-');
    ['stem', 'denoise', 'keychord', 'tempo', 'midi'].forEach((job) => {
      const key = 'browser.lab.' + job;
      items.push({ label: tr(key), sub: 'PRO', action: () => sendToLabJob(f.path, job) });
    });
    items.push('-');
  }
  items.push({ label: tr('browser.sendLab'), action: () => sendToLabJob(f.path, 'analyze') });
  items.push({
    label: '★ / ☆',
    action: () => bridge('browser.toggleFavorite', { path: f.path })
      .then((nowFav) => { if (nowFav) state.favSet.add(f.path); else state.favSet.delete(f.path); renderTree(); paintFromRaw(true); }),
  });
  items.push('-');
  items.push({
    label: tr('browser.ctx.copyPath'),
    action: () => { navigator.clipboard?.writeText(f.path); toast(tr('toast.copied')); },
  });
  items.push({ label: tr('browser.ctx.reveal'), action: () => bridge('reaper.reveal', { path: f.path }) });
  items.push('-');
  items.push({ label: tr('browser.ctx.rename'), sub: 'F2', action: () => startRename(f) });
  items.push({ label: tr('browser.ctx.delete'), sub: 'Del', action: () => startDelete(f) });
  showMenu(e, items);
  const m = $('#ctxMenu');
  const tagSwatchRow = el('div', 'ctx-item', tr('browser.ctx.tag'));
  const swatches = el('span');
  swatches.style.display = 'flex';
  swatches.style.gap = '4px';
  swatches.style.marginLeft = 'auto';
  for (let i = 0; i <= 7; ++i) {
    const s = el('span', 'tag-swatch');
    s.style.background = i === 0 ? 'transparent' : TAG_COLORS[i];
    if (i === 0) s.style.border = '1px solid var(--text-disabled)';
    s.onclick = (ev) => {
      ev.stopPropagation();
      bridge('browser.tag', { path: f.path, color: i });
      if (state.tagCache) { if (i > 0) state.tagCache[f.path] = i; else delete state.tagCache[f.path]; }
      m.classList.add('hidden');
      paintFromRaw(true);
    };
    swatches.appendChild(s);
  }
  tagSwatchRow.appendChild(swatches);
  m.appendChild(el('div', 'ctx-sep'));
  m.appendChild(tagSwatchRow);
}

function folderMenu(e, path) {
  const isRoot = state.roots.some((r) => r.path === path);
  const name = path.split(/[\\/]/).filter(Boolean).pop() || path;
  const items = [
    { label: tr('browser.ctx.scanNew'), action: () => triggerFolderScan(path, false) },
    { label: tr('browser.ctx.rescanAll'), action: () => triggerFolderScan(path, true) },
    { label: tr('browser.ctx.openHere'), action: () => { state.expanded.add(path); openDir(path); renderTree(); } },
    { label: tr('browser.ctx.setRoot'), action: () => bridge('fs.addRoot', { name, path }).then(() => refreshRoots()).then(() => toast(tr('toast.rootAdded'))) },
    { label: tr('browser.ctx.copyPath'), action: () => { navigator.clipboard?.writeText(path); toast(tr('toast.copied')); } },
    { label: tr('browser.ctx.reveal'), action: () => bridge('reaper.reveal', { path }) },
  ];
  if (isRoot) {
    items.push('-');
    items.push({ label: tr('browser.ctx.removeRoot'), action: () => bridge('fs.removeRoot', { path }).then(() => refreshRoots()).then(renderTree) });
  }
  showMenu(e, items);
}

// ============ Market (mock — API Phase 3) ============
function productEl(p) {
  const d = el('div', 'product');
  const th = el('div', 'thumb', p.ini);
  th.style.background = p.c;
  d.appendChild(th);
  const info = el('div', 'info');
  info.appendChild(el('div', 'pname', p.n));
  const meta = el('div', 'meta');
  if (p.tag) meta.appendChild(el('span', 'tag ' + p.tag.cls, p.tag.text));
  meta.appendChild(el('span', '', p.metaText));
  info.appendChild(meta);
  d.appendChild(info);
  const btn = el('button', 'btn-card ' + (p.btnSecondary ? 'secondary' : 'primary'), p.btnText);
  if (p.btnDisabled) btn.disabled = true;
  else btn.onclick = () => toast(tr('market.apiStub'));
  d.appendChild(btn);
  return d;
}
function renderMarket() {
  const trending = [
    { n: 'MegaGrit Multiband Distortion', ini: 'MG', c: '#5b4a6b', tag: { cls: 'free', text: 'FREE' }, metaText: 'JSFX · Bao phuc Nguyen', btnText: tr('market.download') },
    { n: 'Hackey Trackey for REAPER', ini: 'HT', c: '#3d5a52', tag: { cls: 'free', text: 'FREE' }, metaText: 'ReaScript Lua · Bao phuc Nguyen', btnText: tr('market.download') },
    { n: 'JSFX Mastering Suite', ini: 'MS', c: '#3e4c6b', tag: { cls: 'pro', text: 'PRO' }, metaText: 'JSFX · Bao phuc Nguyen', btnText: '199K', btnSecondary: true },
    { n: 'Yumyoo Beatmaker Toolkit', ini: 'YT', c: '#524058', tag: { cls: 'pro', text: 'PRO' }, metaText: 'ReaScript Lua · Bao phuc Nguyen', btnText: '249K', btnSecondary: true },
  ];
  const installed = [
    { n: 'Properties Ribbon', ini: 'PR', c: '#4a5a52', tag: { cls: 'upd', text: tr('market.tagUpdate') + ' 2.1' }, metaText: 'v2.0 · ' + tr('market.installedNote'), btnText: tr('update.button') },
    { n: 'ReaBlink Ableton Link', ini: 'RB', c: '#5a4040', metaText: 'v1.3 · ' + tr('market.installedNote'), btnText: '✓', btnSecondary: true, btnDisabled: true },
  ];
  const box = $('#marketList');
  box.innerHTML = '';
  trending.forEach((p) => box.appendChild(productEl(p)));
  const inst = $('#installedList');
  inst.innerHTML = '';
  installed.forEach((p) => inst.appendChild(productEl(p)));
  $$('.chips .chip').forEach((chip) => {
    chip.onclick = () => {
      $$('.chips .chip').forEach((c) => c.classList.remove('on'));
      chip.classList.add('on');
    };
  });
}

// ============ Audio Lab (REAL API) ============
const labState = { file: null, running: false };
function setLabFile(path) {
  labState.file = path;
  const box = $('#labFile');
  box.innerHTML = '';
  const name = path.split(/[\\/]/).pop();
  box.appendChild(el('b', '', name));
  box.appendChild(el('div', 'muted', path));
}
function labProgress(show, percent, stage) {
  const box = $('#labProgress');
  box.classList.toggle('hidden', !show);
  if (show) {
    $('#labProgressFill').style.width = (percent || 0) + '%';
    $('#labProgressText').textContent = stage || '';
  }
}
function initLab() {
  $$('#labGrid .lab-tool').forEach((b) => {
    b.onclick = () => {
      if (!labState.file) { toast(tr('lab.noFile')); return; }
      if (labState.running) { toast(tr('lab.alreadyRunning')); return; }
      labState.running = true;
      labProgress(true, 0, '...');
      const job = b.dataset.job;
      const args = { path: labState.file };
      if (job === 'stem') args.mode = 4;
      if (job === 'denoise') args.strength = 80;
      bridge('lab.' + job, args).catch((e) => { labState.running = false; labProgress(false); toast(e.message); });
    };
  });
}
function renderLabResult(d) {
  labState.running = false;
  labProgress(false);
  const box = $('#labResults');
  box.innerHTML = '';
  const card = el('div', 'result-card');
  if (d.job === 'analyze') {
    const p = d.payload || {};
    card.innerHTML = '';
    card.appendChild(el('h4', '', 'Tempo & Key'));
    card.appendChild(el('div', 'kbd-line', `BPM: ${p.bpm} · Key: ${p.master_key} ${p.scale_mode || ''} · ${p.duration}s`));
  } else if (d.job === 'keychord') {
    const p = d.payload || {};
    card.appendChild(el('h4', '', 'Key & Hợp âm'));
    const tel = p.telemetry || {};
    card.appendChild(el('div', 'kbd-line', `Key: ${tel.master_key} ${tel.scale_mode || ''} · BPM: ${tel.bpm} · ${tel.time_signature || ''}`));
    const chords = (p.chords || []).slice(0, 24).map((c) => c.chord).join(' — ');
    card.appendChild(el('div', 'muted', chords || '(no chords)'));
  } else if (d.job === 'stem' || d.job === 'denoise') {
    card.appendChild(el('h4', '', d.job === 'stem' ? 'Kết quả tách stem' : 'Audio đã lọc noise'));
    const paths = [];
    (d.files || []).forEach((f) => {
      const row = el('div', 'stem-row');
      armOleDrag(row, { path: f.path, isDir: false });
      const dot = el('span', 'tagdot');
      dot.style.background = f.color || '#ccc';
      row.appendChild(dot);
      row.appendChild(el('span', 'sn', f.name));
      const ins = el('a', '', '+ vào project');
      ins.onclick = () => insertMedia(f.path);
      row.appendChild(ins);
      card.appendChild(row);
      paths.push(f.path);
    });
    if (d.zipPath) {
      const row = el('div', 'stem-row');
      row.appendChild(el('span', 'sn', 'ZIP'));
      const ins = el('a', '', 'đã tải — mở thư mục');
      ins.onclick = () => bridge('reaper.reveal', { path: d.zipPath });
      row.appendChild(ins);
      card.appendChild(row);
    }
    if (paths.length) {
      const btn = el('button', 'accent', 'Chèn tất cả vào project');
      btn.onclick = () => bridge('reaper.insertMany', { paths }).then(() => toast(tr('toast.inserted')));
      card.appendChild(btn);
    }
  }
  box.appendChild(card);
}

// ============ Agent (mock — API Phase 5) ============
function agentSay(text, cls = 'bot') {
  const chat = $('#chat');
  chat.appendChild(el('div', 'bubble ' + cls, text));
  chat.scrollTop = chat.scrollHeight;
}
function initAgent() {
  agentSay('Xin chào! Mình là trợ lý AI của Reals Lab. (Phase 5 — demo giao diện)');
  $('#btnSend').onclick = () => {
    const inp = $('#chatInput');
    if (!inp.value.trim()) return;
    agentSay(inp.value, 'user');
    inp.value = '';
    setTimeout(() => agentSay('Mình sẽ trả lời khi API LLM được nối ở Phase 5. Mọi lệnh điều khiển REAPER sẽ chạy qua bridge đã sẵn sàng!'), 400);
  };
  $('#chatInput').addEventListener('keydown', (e) => { if (e.key === 'Enter') $('#btnSend').click(); });
  $$('#agentMode button').forEach((b) => {
    b.onclick = () => { $$('#agentMode button').forEach((x) => x.classList.remove('on')); b.classList.add('on'); };
  });
}

// ============ Drag & Drop (preview/mock only) ============
// In WebView2, AllowExternalDrop is FALSE — Explorer drops are handled by
// native IDropTarget → fs.dropPaths / fs.dropHover. JS File.path is not a
// real OS path there, so this HTML5 path is preview-only.
function initDragAndDrop() {
  if (hasWebView) return;
  let dragDepth = 0;
  const overlay = $('#dropOverlay');
  const isFileDrag = (e) => {
    const types = e.dataTransfer && e.dataTransfer.types;
    return !!(types && Array.from(types).includes('Files'));
  };

  window.addEventListener('dragenter', (e) => {
    if (!isFileDrag(e)) return;
    e.preventDefault();
    dragDepth++;
    if (overlay) overlay.classList.remove('hidden');
  });

  window.addEventListener('dragover', (e) => {
    if (!isFileDrag(e)) return;
    e.preventDefault();
    e.dataTransfer.dropEffect = 'copy';
  });

  window.addEventListener('dragleave', (e) => {
    dragDepth = Math.max(0, dragDepth - 1);
    if ((dragDepth === 0 || !e.relatedTarget) && overlay) {
      dragDepth = 0;
      overlay.classList.add('hidden');
    }
  });

  window.addEventListener('dragend', () => {
    dragDepth = 0;
    if (overlay) overlay.classList.add('hidden');
  });

  window.addEventListener('drop', (e) => {
    e.preventDefault();
    dragDepth = 0;
    if (overlay) overlay.classList.add('hidden');
    const items = e.dataTransfer && e.dataTransfer.items;
    const paths = [];
    if (items) {
      for (let i = 0; i < items.length; ++i) {
        const it = items[i];
        if (it.kind !== 'file') continue;
        const entry = it.webkitGetAsEntry && it.webkitGetAsEntry();
        const file = it.getAsFile && it.getAsFile();
        const name = (entry && entry.name) || (file && file.name);
        if (!name) continue;
        if (entry && !entry.isDirectory) continue;
        paths.push('D:\\Dropped\\' + name.replace(/\.[^.]+$/, ''));
      }
    }
    if (paths.length) bridge('fs.dropPaths', { paths }).catch(() => {});
    else handleEvent('fs.rootsChanged', { added: [] });
  });
}

// ============ Boot ============
async function initVersion() {
  try {
    const info = await bridge('app.info');
    if (info && info.version) {
      const vl = $('#verLabel');
      if (vl) vl.textContent = `Reals Lab v${info.version}`;
    }
  } catch { /* keep static fallback text */ }
}

async function boot() {
  buildSettingsPop();
  applyStubVisibility();
  await initSettings();
  renderNav();
  showTab(state.tab);
  renderMarket();
  initAgent();
  initLab();
  await initBrowser();
  initVersion();
  initDragAndDrop();

  const btnDock = $('#btnDock');
  if (btnDock) btnDock.onclick = () => bridge('window.toggleDock');
  const btnMin = $('#btnWinMin') || $('#btnMin');
  if (btnMin) btnMin.onclick = () => bridge('window.minimize');
  const btnMax = $('#btnWinMax');
  if (btnMax) btnMax.onclick = () => bridge('window.toggleMaximize');
  const btnClose = $('#btnWinClose') || $('#btnClose');
  if (btnClose) btnClose.onclick = () => bridge('window.close');

  bridge('window.isDocked').then((res) => {
    if (res && typeof res.docked === 'boolean') {
      applyDockState(res.docked);
    }
  });

  const winDrag = $('#winDragRegion');
  if (winDrag) {
    winDrag.addEventListener('mousedown', (e) => {
      if (e.button === 0) bridge('window.startDrag');
    });
  }

  $$('.resize-handle').forEach((h) => {
    h.addEventListener('mousedown', (e) => {
      if (e.button === 0) {
        e.preventDefault();
        e.stopPropagation();
        const edge = h.className.replace('resize-handle', '').trim();
        bridge('window.startResize', { edge });
      }
    });
  });

  const btnToggleTree = $('#btnToggleTree');
  if (btnToggleTree) {
    btnToggleTree.onclick = () => {
      const tree = $('#tree');
      if (tree) {
        tree.classList.toggle('collapsed');
        const collapsed = tree.classList.contains('collapsed');
        btnToggleTree.classList.toggle('on', !collapsed);
        drawWaveform();
      }
    };
  }

  initLayoutSplitters();

  window.addEventListener('resize', () => {
    drawWaveform();
    drawMeter();
  });

  const btnLogin = $('#btnLogin');
  if (btnLogin) btnLogin.onclick = () => toast(tr('account.apiStub'));
}

function initLayoutSplitters() {
  // 1. Tree Vertical Splitter
  const treeSplitter = $('#treeSplitter');
  const tree = $('#tree');
  const browserBody = $('.browser-body');

  if (treeSplitter && tree && browserBody) {
    treeSplitter.addEventListener('mousedown', (e) => {
      if (e.button !== 0) return;
      e.preventDefault();
      e.stopPropagation();
      treeSplitter.classList.add('dragging');
      document.body.classList.add('resizing-col');

      const startX = e.clientX;
      const startW = tree.getBoundingClientRect().width;

      const onMove = (moveEv) => {
        const delta = moveEv.clientX - startX;
        const maxW = Math.max(100, browserBody.clientWidth - 80);
        const newW = Math.min(Math.max(50, startW + delta), maxW);
        tree.style.width = newW + 'px';
        drawWaveform();
      };

      const onUp = () => {
        treeSplitter.classList.remove('dragging');
        document.body.classList.remove('resizing-col');
        window.removeEventListener('mousemove', onMove);
        window.removeEventListener('mouseup', onUp);
        const finalW = tree.getBoundingClientRect().width;
        bridge('config.set', { key: 'treeWidth', value: Math.round(finalW) });
      };

      window.addEventListener('mousemove', onMove);
      window.addEventListener('mouseup', onUp);
    });
  }

  // 2. Preview Horizontal Splitter
  const previewSplitter = $('#previewSplitter');
  const preview = $('#preview');
  const waveform = $('#waveform');
  const paneBrowser = $('#pane-browser');

  if (previewSplitter && preview && paneBrowser) {
    previewSplitter.addEventListener('mousedown', (e) => {
      if (e.button !== 0) return;
      e.preventDefault();
      e.stopPropagation();
      previewSplitter.classList.add('dragging');
      document.body.classList.add('resizing-row');

      const startY = e.clientY;
      const startH = preview.getBoundingClientRect().height;
      const startWfH = waveform ? waveform.clientHeight : 38;

      const onMove = (moveEv) => {
        const delta = startY - moveEv.clientY;
        const maxH = Math.max(120, paneBrowser.clientHeight - 120);
        const newH = Math.min(Math.max(70, startH + delta), maxH);
        preview.style.height = newH + 'px';
        if (waveform) {
          const newWfH = Math.min(Math.max(24, startWfH + delta), 160);
          waveform.style.height = newWfH + 'px';
          waveform.height = newWfH;
        }
        drawWaveform();
        drawMeter();
      };

      const onUp = () => {
        previewSplitter.classList.remove('dragging');
        document.body.classList.remove('resizing-row');
        window.removeEventListener('mousemove', onMove);
        window.removeEventListener('mouseup', onUp);
        const finalH = preview.getBoundingClientRect().height;
        bridge('config.set', { key: 'previewHeight', value: Math.round(finalH) });
      };

      window.addEventListener('mousemove', onMove);
      window.addEventListener('mouseup', onUp);
    });
  }
}

boot();
