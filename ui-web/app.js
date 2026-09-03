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
    'market.buy': 'Mua Pro',
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
    'settings.title': 'Cài đặt Reals Lab',
    'settings.tab.general': 'General', 'settings.tab.browser': 'File Browser',
    'settings.tab.market': 'Marketplace', 'settings.tab.stem': 'Reals Stem Separation',
    'settings.tab.agent': 'Agent AI',
    'market.comingSoon': 'Marketplace — Sắp ra mắt',
    'market.placeholderDesc': 'Kho thư viện Sample Pack, Preset và Plugin bản quyền từ cộng đồng sáng tạo Reals.',
    'stem.comingSoon': 'Reals Stem Separation — Sắp ra mắt',
    'stem.placeholderDesc': 'Tách Vocals, Drums, Bass, Instruments chuẩn xác bằng AI tích hợp trực tiếp trong REAPER.',
    'agent.comingSoon': 'Agent AI — Sắp ra mắt',
    'agent.placeholderDesc': 'Trợ lý AI tự động hóa chỉnh sửa, định tuyến và sáng tác thông minh trong DAW.',
    'common.enabled': 'Bật', 'common.disabled': 'Tắt',
    'settings.navPosition': 'Vị trí thanh điều hướng', 'settings.theme': 'Giao diện (Theme)', 'theme.darkStudio': 'Dark Studio', 'theme.pastelPink': 'Pastel Pink', 'theme.cyberpunk': 'Cyberpunk', 'settings.accent': 'Màu chủ đạo', 'settings.language': 'Ngôn ngữ',
    'settings.effects': 'Hiệu ứng', 'settings.noise': 'Lớp phủ noise',
    'settings.browser': 'Thư mục & Duyệt', 'settings.autoCollapse': 'Tự động đóng thư mục khác',
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
    'player.dragTip': 'Kéo vào REAPER',
    'browser.loadingSamples': 'Đang tải sample...',
    'browser.addFolder': 'Thêm thư mục Sample',
    'scanner.cancel': 'Dừng',
    'settings.miniWave': 'Sóng âm mini dưới tên file',
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
    'market.buy': 'Get Pro',
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
    'settings.title': 'Reals Lab Settings',
    'settings.tab.general': 'General', 'settings.tab.browser': 'File Browser',
    'settings.tab.market': 'Marketplace', 'settings.tab.stem': 'Reals Stem Separation',
    'settings.tab.agent': 'Agent AI',
    'market.comingSoon': 'Marketplace — Coming Soon',
    'market.placeholderDesc': 'Library of Sample Packs, Presets, and Plugins from the Reals creative community.',
    'stem.comingSoon': 'Reals Stem Separation — Coming Soon',
    'stem.placeholderDesc': 'Precise AI-powered stem separation directly inside REAPER.',
    'agent.comingSoon': 'Agent AI — Coming Soon',
    'agent.placeholderDesc': 'Intelligent AI assistant for DAW automation, routing, and composition.',
    'common.enabled': 'Enabled', 'common.disabled': 'Disabled',
    'settings.navPosition': 'Navigation Position', 'settings.theme': 'Theme', 'theme.darkStudio': 'Dark Studio', 'theme.pastelPink': 'Pastel Pink', 'theme.cyberpunk': 'Cyberpunk', 'settings.accent': 'Accent Color', 'settings.language': 'Language',
    'settings.effects': 'Effects', 'settings.noise': 'Noise overlay',
    'settings.browser': 'Folder tree', 'settings.autoCollapse': 'Auto-collapse other folders',
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
    'player.dragTip': 'Drag to REAPER',
    'browser.loadingSamples': 'Loading samples...',
    'browser.addFolder': 'Add Sample Folder',
    'scanner.cancel': 'Stop',
    'settings.miniWave': 'Mini waveform preview in file list',
    'browser.dropTitle': 'Add Root Folder',
    'browser.dropHint': 'Drop folder from Windows Explorer here to add as root',
    'lab.alreadyRunning': 'A job is already running — please wait',
  },
};
let LANG = 'vi';
const tr = (k) => (I18N[LANG] && I18N[LANG][k]) || I18N.vi[k] || k;

// ============ Theme Engine & Canvas Color Sync ============
const canvasThemeColors = {
  waveformBg: '#0B0E14',
  waveformFill: 'rgba(255, 255, 255, 0.12)',
  waveformFillActive: 'rgba(56, 189, 248, 0.75)',
  waveformPlayhead: 'rgba(255, 255, 255, 0.85)',
  waveformCenterline: 'rgba(255, 255, 255, 0.05)',
  meterBg: '#0B0E14',
  meterFill: '#35D07F',
  meterFillWarn: '#F59E0B',
  meterFillClip: '#FF5C66',
  pianorollBg: '#0B0E14',
  pianorollGrid: 'rgba(255, 255, 255, 0.04)',
  pianorollNote: '#38BDF8',
  pianorollNoteActive: '#FFFFFF',
  pianorollNoteGradEnd: '#0284C7',
};

function updateCanvasThemeColors(tokens) {
  if (!tokens) return;
  if (tokens.waveformBg) canvasThemeColors.waveformBg = tokens.waveformBg;
  if (tokens.waveformFill) canvasThemeColors.waveformFill = tokens.waveformFill;
  if (tokens.waveformFillActive) canvasThemeColors.waveformFillActive = tokens.waveformFillActive;
  if (tokens.waveformPlayhead) canvasThemeColors.waveformPlayhead = tokens.waveformPlayhead;
  if (tokens.waveformCenterline) canvasThemeColors.waveformCenterline = tokens.waveformCenterline;
  if (tokens.meterBg) canvasThemeColors.meterBg = tokens.meterBg;
  if (tokens.meterFill) canvasThemeColors.meterFill = tokens.meterFill;
  if (tokens.meterFillWarn) canvasThemeColors.meterFillWarn = tokens.meterFillWarn;
  if (tokens.meterFillClip) canvasThemeColors.meterFillClip = tokens.meterFillClip;
  if (tokens.pianorollBg) canvasThemeColors.pianorollBg = tokens.pianorollBg;
  if (tokens.pianorollGrid) canvasThemeColors.pianorollGrid = tokens.pianorollGrid;
  if (tokens.pianorollNote) canvasThemeColors.pianorollNote = tokens.pianorollNote;
  if (tokens.pianorollNoteActive) canvasThemeColors.pianorollNoteActive = tokens.pianorollNoteActive;
  if (tokens.pianorollNoteGradEnd) canvasThemeColors.pianorollNoteGradEnd = tokens.pianorollNoteGradEnd;
}

window.addEventListener('themeUpdated', (e) => {
  if (e && e.detail && e.detail.tokens) {
    updateCanvasThemeColors(e.detail.tokens);
  }
  if (typeof drawWaveform === 'function') {
    drawWaveform();
  }
  if (typeof drawMeterSmoothed === 'function') {
    drawMeterSmoothed((typeof _meterSmoothedVal !== 'undefined' ? _meterSmoothedVal : 0) || (typeof state !== 'undefined' && state ? state.peak : 0) || 0);
  }
});

class ThemeManager {
  constructor() {
    this._validThemes = ['dark-studio', 'pastel-pink', 'cyberpunk'];
    this._currentTheme = 'dark-studio';

    try {
      const saved = localStorage.getItem('reals_theme');
      if (saved && this._validThemes.includes(saved)) {
        this._currentTheme = saved;
      }
    } catch (e) {
      // ignore
    }

    this.applyTheme(this._currentTheme, false);

    if (window.chrome && window.chrome.webview && window.chrome.webview.addEventListener) {
      window.chrome.webview.addEventListener('message', (e) => {
        const data = e.data;
        if (typeof data === 'string' && data.startsWith('THEME_CHANGED:')) {
          const themeName = data.slice('THEME_CHANGED:'.length).trim();
          if (themeName && this._validThemes.includes(themeName)) {
            this.applyTheme(themeName, false);
          }
        }
      });
    }
  }

  getTheme() {
    return this._currentTheme;
  }

  applyTheme(themeName, notifyNative = false) {
    if (!themeName || !this._validThemes.includes(themeName)) {
      themeName = 'dark-studio';
    }
    this._currentTheme = themeName;

    try {
      localStorage.setItem('reals_theme', themeName);
      if (typeof bridge === 'function' && typeof hasWebView !== 'undefined' && hasWebView) {
        bridge('config.set', { key: 'theme', value: themeName });
      }
    } catch (e) {
      // ignore
    }

    // Clean up conflicting inline accent properties so theme tokens take effect
    const rootEl = document.documentElement;
    const accentProps = [
      '--accent', '--accent-hover', '--accent-active',
      '--accent-soft', '--accent-border', '--accent-focus', '--accent-glow'
    ];
    accentProps.forEach((prop) => rootEl.style.removeProperty(prop));

    rootEl.setAttribute('data-theme', themeName);

    try {
      const styles = getComputedStyle(rootEl);
      const tokens = {
        bgApp: styles.getPropertyValue('--bg-app').trim(),
        bgRoot: styles.getPropertyValue('--bg-root').trim(),
        bgSidebar: styles.getPropertyValue('--bg-sidebar').trim(),
        bgPanel: styles.getPropertyValue('--bg-panel').trim(),
        bgCard: styles.getPropertyValue('--bg-card').trim(),
        accent: styles.getPropertyValue('--accent').trim(),
        accentHover: styles.getPropertyValue('--accent-hover').trim(),
        textPrimary: styles.getPropertyValue('--text-primary').trim(),
        textSecondary: styles.getPropertyValue('--text-secondary').trim(),
        textTertiary: styles.getPropertyValue('--text-tertiary').trim(),
        borderSubtle: styles.getPropertyValue('--border-subtle').trim(),
        borderDefault: styles.getPropertyValue('--border-default').trim(),
        waveformFill: styles.getPropertyValue('--waveform-fill').trim(),
        waveformFillActive: styles.getPropertyValue('--waveform-fill-active').trim(),
        waveformBg: styles.getPropertyValue('--waveform-bg').trim(),
        waveformPlayhead: styles.getPropertyValue('--waveform-playhead').trim(),
        waveformCenterline: styles.getPropertyValue('--waveform-centerline').trim(),
        meterBg: styles.getPropertyValue('--meter-bg').trim(),
        meterFill: styles.getPropertyValue('--meter-fill').trim(),
        meterFillWarn: styles.getPropertyValue('--meter-fill-warn').trim(),
        meterFillClip: styles.getPropertyValue('--meter-fill-clip').trim(),
        pianorollBg: styles.getPropertyValue('--pianoroll-bg').trim(),
        pianorollGrid: styles.getPropertyValue('--pianoroll-grid').trim(),
        pianorollNote: styles.getPropertyValue('--pianoroll-note').trim(),
        pianorollNoteActive: styles.getPropertyValue('--pianoroll-note-active').trim(),
        pianorollNoteGradEnd: styles.getPropertyValue('--pianoroll-note-grad-end').trim(),
      };
      updateCanvasThemeColors(tokens);
      window.dispatchEvent(new CustomEvent('themeUpdated', { detail: { theme: themeName, tokens } }));
    } catch (e) {
      // ignore
    }

    if (notifyNative && window.chrome && window.chrome.webview && window.chrome.webview.postMessage) {
      try {
        window.chrome.webview.postMessage('THEME_CHANGED:' + themeName);
      } catch (e) {
        console.warn('ThemeManager: postMessage failed', e);
      }
    }
  }
}

window.themeManager = new ThemeManager();

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
    { name: 'Trap_Melody_8bar.mid', path: 'D:\\Samples\\Trap_Melody_8bar.mid', isAudio: true, size: 4200, modified: 1787570000 },
    { name: 'Chord_Progression_Am.mid', path: 'D:\\Samples\\Chord_Progression_Am.mid', isAudio: true, size: 3800, modified: 1787575000 },
    { name: 'Drums', path: 'D:\\Samples\\Drums', isAudio: false, isDir: true, size: 0, modified: 1787500000 },
    { name: 'Readme_Sample_Pack.txt', path: 'D:\\Samples\\Readme_Sample_Pack.txt', isAudio: false, isDir: false, size: 1200, modified: 1787500000 }
  ],
  favorites: ['D:\\Samples\\Kick_Punchy_01.wav', 'D:\\Samples\\Vocal_Hook_128bpm_Am.wav'],
  recents: ['D:\\Samples\\Snare_808_Clean.wav', 'D:\\Samples\\Bass_Sub_Deep_F.wav', 'D:\\Samples\\Trap_Melody_8bar.mid'],
  tags: { 'D:\\Samples\\Kick_Punchy_01.wav': 3, 'D:\\Samples\\Vocal_Hook_128bpm_Am.wav': 5, 'D:\\Samples\\Trap_Melody_8bar.mid': 2 }
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
            { name: 'Drum_Groove.mid', path: p + '\\Drum_Groove.mid', isAudio: true, isDir: false, size: 5400, modified: 1787530000 },
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
      else if (cmd === 'browser.getFavoriteEntries' || cmd === 'browser.favorites.listEntries' || cmd === 'browser.listFavorites') {
        const favPaths = new Set(mockStore.favorites || []);
        const files = (mockStore.files || []).filter((f) => !f.isDir && favPaths.has(f.path));
        resolve({ files });
      }
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
        // Preserve the key's case so minor mode (e.g. "Am", "F#m") stays
        // distinguishable from major ("A", "F#"). Previously `.toUpperCase()`
        // turned "Am" into "AM", which then rendered as "Root: AM" in the
        // piano transposer badge instead of "Root: Am".
        const km = p.match(/_([A-G][#b]?(?:m|maj|min|minor|major)?)(?:_|\.|$)/i);
        let key = '';
        if (km) {
          // Capitalize only the note letter, keep the mode suffix lowercase.
          key = km[1].charAt(0).toUpperCase() + km[1].slice(1).toLowerCase();
          // Normalize long-form modes to short "m" for minor.
          key = key.replace(/(maj|min|minor|major)$/, (s) => (s.startsWith('min') || s === 'm' ? 'm' : s));
        }
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
      } else if (cmd === 'search.findSimilar' || cmd === 'ai.findSimilar' || cmd === 'browser.findSimilar') {
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
        resolve({ ok: true, duration: 4.8, sampleRate: 44100, channels: 2, envelope: fakeEnv });
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
    if (typeof m === 'string' && m.startsWith('THEME_CHANGED:')) {
      const theme = m.slice('THEME_CHANGED:'.length).trim();
      if (window.themeManager) {
        window.themeManager.applyTheme(theme, false);
      }
      return;
    }
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
function isMidiFile(f) {
  if (!f) return false;
  if (typeof f === 'string') return /\.(mid|midi)$/i.test(f);
  return f.ext === 'mid' || f.ext === 'midi' || /\.(mid|midi)$/i.test(f.name || '') || /\.(mid|midi)$/i.test(f.path || '');
}
function normPath(p) {
  if (!p) return '';
  return String(p).replace(/[\/\\]+/g, '\\').replace(/\\+$/, '');
}
function isSamePath(a, b) {
  return normPath(a).toLowerCase() === normPath(b).toLowerCase();
}
function isPathUnder(child, parent) {
  const c = normPath(child).toLowerCase();
  const p = normPath(parent).toLowerCase();
  if (c === p) return true;
  return c.startsWith(p + '\\');
}
function parentDir(path) {
  if (!path) return path;
  const n = normPath(path);
  const i = n.lastIndexOf('\\');
  return i <= 0 ? n : n.slice(0, i);
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
  timeRatio: 1,
  loop: false, sort: 0, expanded: new Set(), tab: 'browser',
  tagCache: {}, favSet: new Set(),
  listSeq: 0, searchSeq: 0, treeSeq: 0,
  autoPreview: true, favOnly: false, tagFilter: 0,
  rawFiles: [], files: [], listDir: null,
  searchQ: '', searchPending: false, searchGen: 0,
  similarSource: null, similarSourceName: null,
  probeCache: {}, probeInflight: new Set(),
  syncBpm: false, pitchSemitones: 0,
  originalRootNote: 'C', selectedTargetNote: 'C',
  sampleBpm: 0, sampleKey: 'ORIGINAL', sampleMode: '',
  sampleTags: [], midiNotes: [],
  autoCollapseTree: false,
  displaySize: 'medium',
  subCache: {},
  dirScrolls: {},
  _dragArm: null,
  _suppressClick: false,
  _dropHoverT: null,
  _watchT: null,
  _previewArmT: null,
  _accentCache: '#38BDF8',
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
    // Per-dir debounce: previously a single timer was reused, so a second
    // folder change within 250ms cleared the first folder's pending reload
    // entirely (its subCache stayed stale). Now each dir keeps its own timer
    // so concurrent watches from multiple folders all refresh correctly.
    if (!state._watchTimers) state._watchTimers = new Map();
    const timers = state._watchTimers;
    const existing = timers.get(dir);
    if (existing) clearTimeout(existing);
    timers.set(dir, setTimeout(() => {
      timers.delete(dir);
      delete state.subCache[dir];
      if (dir === state.currentDir && !(state.searchQ || '').trim())
        loadDir(state.currentDir, true);
    }, 250));
    return;
  }
  if (event === 'audio.envelope') {
    if (!state.envCache) state.envCache = {};
    if (data && data.path && data.envelope) {
      state.envCache[data.path] = data.envelope;
      updateRowMiniWave(data.path);
    }
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
      // CRIT-KEY-LOCK: Do NOT clobber userTargetNote or pitch when key is locked
      if (typeof data.semitones === 'number' && !state.isUserTargetKeyLocked) {
        state.pitchSemitones = data.semitones;
        updateTransposerPopUI();
      }
    }
    return;
  }
  if (event === 'audio.state') {
    state.playing = !!data.playing;
    state.duration = data.duration || state.duration;
    // Output playback rate (1.0 = raw speed). The playhead advances over the
    // OUTPUT timeline: output duration = raw duration / timeRatio.
    if (typeof data.timeRatio === 'number' && data.timeRatio > 0) {
      state.timeRatio = data.timeRatio;
    }
    // CRIT-KEY-LOCK: C++ audio engine emits periodic audio.state. When the user has locked
    // a target key (e.g. Note A), asynchronous state events MUST NEVER overwrite the user's
    // active pitch shift, otherwise sample transitions will randomly jump back/forth in pitch.
    if (typeof data.pitchSemitones === 'number' && !state.isUserTargetKeyLocked) {
      state.pitchSemitones = data.pitchSemitones;
      updateTransposerPopUI();
    }
    const bp = $('#btnPlay');
    if (bp) {
      bp.textContent = state.playing ? '❚❚' : '▶';
      bp.classList.toggle('playing', state.playing);
    }
    if (state.playing) {
      state.position = data.position || 0;
      state.peak = data.peak || 0;
      startPlayerAnimLoop();
      drawWaveform();
    } else {
      const keepPos = !!(state.syncBpm && typeof data.position === 'number');
      if (keepPos) {
        state.position = data.position;
      }
      state.peak = 0;
      _meterSmoothedVal = 0;
      refreshPlayState(keepPos);
    }
  }
}

// ============ Real Audio / MIDI Synth & Pitch Engine ============
const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

let _audioCtx = null;
let _midiSynthActiveNotes = [];
let _midiPlaybackTimer = null;
let _midiStartTime = 0;
let _midiTotalDuration = 0;
let _currentMidiEvents = [];

function getAudioContext() {
  if (!_audioCtx) {
    const AudioCtx = window.AudioContext || window.webkitAudioContext;
    if (AudioCtx) _audioCtx = new AudioCtx();
  }
  if (_audioCtx && _audioCtx.state === 'suspended') {
    _audioCtx.resume().catch(() => {});
  }
  return _audioCtx;
}

function stopMidiPlayback() {
  if (_midiPlaybackTimer) {
    cancelAnimationFrame(_midiPlaybackTimer);
    _midiPlaybackTimer = null;
  }
  _midiSynthActiveNotes.forEach((n) => {
    try {
      if (n.osc) { n.osc.stop(); n.osc.disconnect(); }
    } catch {}
  });
  _midiSynthActiveNotes = [];
}

function playSynthNote(noteNameOrMidi, duration = 0.35) {
  try {
    const ctx = getAudioContext();
    if (!ctx) return;
    let midiNum = 60;
    if (typeof noteNameOrMidi === 'number') {
      midiNum = noteNameOrMidi;
    } else if (typeof noteNameOrMidi === 'string') {
      const root = extractRootNoteName(noteNameOrMidi);
      const idx = NOTE_NAMES.indexOf(root);
      midiNum = 60 + (idx >= 0 ? idx : 0);
    }
    const freq = 440 * Math.pow(2, (midiNum - 69) / 12);
    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = 'triangle';
    osc.frequency.setValueAtTime(freq, ctx.currentTime);

    const now = ctx.currentTime;
    gain.gain.setValueAtTime(0.01, now);
    gain.gain.exponentialRampToValueAtTime(0.25, now + 0.02);
    gain.gain.exponentialRampToValueAtTime(0.001, now + duration);

    osc.connect(gain);
    gain.connect(ctx.destination);
    osc.start(now);
    osc.stop(now + duration + 0.05);
  } catch (err) {
    console.warn('playSynthNote error:', err);
  }
}

// Convert Standard MIDI File binary ArrayBuffer into discrete timestamped note events
function parseMidiBuffer(arrayBuffer) {
  const data = new Uint8Array(arrayBuffer);
  if (data.length < 14) return { notes: [], duration: 0, tonic: 'C' };
  if (data[0] !== 0x4D || data[1] !== 0x54 || data[2] !== 0x68 || data[3] !== 0x64) {
    return { notes: [], duration: 0, tonic: 'C' };
  }

  const numTracks = (data[10] << 8) | data[11];
  let timeDivision = (data[12] << 8) | data[13];
  if (timeDivision <= 0) timeDivision = 480;

  let offset = 14;
  const rawEvents = [];
  let defaultTempo = 500000; // microseconds per quarter note (120 BPM)

  for (let t = 0; t < numTracks && offset < data.length; ++t) {
    if (offset + 8 > data.length) break;
    if (data[offset] !== 0x4D || data[offset+1] !== 0x54 || data[offset+2] !== 0x72 || data[offset+3] !== 0x6B) {
      offset += 8;
      continue;
    }
    const trackLen = (data[offset+4] << 24) | (data[offset+5] << 16) | (data[offset+6] << 8) | data[offset+7];
    offset += 8;
    const trackEnd = Math.min(data.length, offset + trackLen);
    let curTick = 0;
    let lastStatus = 0;
    const active = {};

    while (offset < trackEnd) {
      let delta = 0;
      while (offset < trackEnd) {
        const b = data[offset++];
        delta = (delta << 7) | (b & 0x7F);
        if (!(b & 0x80)) break;
      }
      curTick += delta;

      if (offset >= trackEnd) break;
      let status = data[offset];
      if (status & 0x80) {
        lastStatus = status;
        offset++;
      } else {
        status = lastStatus;
      }

      const type = status & 0xF0;
      if (type === 0x90) { // Note On
        const note = data[offset++];
        const vel = data[offset++];
        if (vel > 0) {
          active[note] = { tick: curTick, vel: vel / 127 };
        } else if (active[note]) {
          rawEvents.push({
            note,
            startTick: active[note].tick,
            endTick: curTick,
            vel: active[note].vel
          });
          delete active[note];
        }
      } else if (type === 0x80) { // Note Off
        const note = data[offset++];
        offset++;
        if (active[note]) {
          rawEvents.push({
            note,
            startTick: active[note].tick,
            endTick: curTick,
            vel: active[note].vel
          });
          delete active[note];
        }
      } else if (status === 0xFF) {
        if (offset >= trackEnd) break;
        const metaType = data[offset++];
        let len = 0;
        while (offset < trackEnd) {
          const b = data[offset++];
          len = (len << 7) | (b & 0x7F);
          if (!(b & 0x80)) break;
        }
        if (metaType === 0x51 && len === 3 && offset + 3 <= trackEnd) {
          defaultTempo = (data[offset] << 16) | (data[offset+1] << 8) | data[offset+2];
        }
        offset += len;
      } else if (type === 0xC0 || type === 0xD0) {
        offset += 1;
      } else if (type === 0xA0 || type === 0xB0 || type === 0xE0) {
        offset += 2;
      }
    }

    for (const note in active) {
      rawEvents.push({
        note: +note,
        startTick: active[note].tick,
        endTick: Math.max(active[note].tick + timeDivision / 2, curTick),
        vel: active[note].vel
      });
    }

    offset = trackEnd;
  }

  const secondsPerTick = (defaultTempo / 1000000) / timeDivision;
  const notes = [];
  let maxTime = 0;
  const pitchHistogram = new Array(12).fill(0);

  rawEvents.forEach((ev) => {
    const time = ev.startTick * secondsPerTick;
    const dur = Math.max(0.08, (ev.endTick - ev.startTick) * secondsPerTick);
    maxTime = Math.max(maxTime, time + dur);
    pitchHistogram[ev.note % 12] += 1;
    notes.push({
      note: ev.note,
      time,
      duration: dur,
      vel: ev.vel
    });
  });

  notes.sort((a, b) => a.time - b.time);

  let bestPitch = 0, bestCount = -1;
  for (let p = 0; p < 12; ++p) {
    if (pitchHistogram[p] > bestCount) {
      bestCount = pitchHistogram[p];
      bestPitch = p;
    }
  }
  const tonic = NOTE_NAMES[bestPitch] || 'C';

  return {
    notes,
    duration: Math.max(1.0, maxTime),
    tonic
  };
}

async function loadMidiPreviewData(path) {
  if (!path || !isMidiFile(path)) return null;
  if (!state._midiCache) state._midiCache = {};
  if (state._midiCache[path]) {
    return state._midiCache[path];
  }

  try {
    const res = await bridge('audio.readMidi', { path });
    const b64 = (typeof res === 'object' && res) ? (res.base64 || res.data?.base64) : null;
    if (b64) {
      const binStr = atob(b64);
      const bytes = new Uint8Array(binStr.length);
      for (let i = 0; i < binStr.length; ++i) bytes[i] = binStr.charCodeAt(i);
      const parsed = parseMidiBuffer(bytes.buffer);
      if (parsed && parsed.notes && parsed.notes.length > 0) {
        state._midiCache[path] = parsed;
        return parsed;
      }
    }
  } catch (err) {
    console.warn('Failed to load MIDI preview:', err);
  }
  return null;
}

function playMidiEvents(notes, totalDur) {
  stopMidiPlayback();
  const ctx = getAudioContext();
  if (!ctx || !notes || !notes.length) return;

  _currentMidiEvents = notes;
  _midiTotalDuration = totalDur;
  _midiStartTime = ctx.currentTime;
  state.playing = true;
  state.duration = totalDur;
  state.position = 0;

  const bp = $('#btnPlay');
  if (bp) {
    bp.textContent = 'II';
    bp.classList.add('playing');
  }

  const shift = state.pitchSemitones || 0;

  notes.forEach((n) => {
    const noteStart = _midiStartTime + n.time;
    const noteDur = n.duration;
    const transposedMidi = Math.max(12, Math.min(127, n.note + shift));
    const freq = 440 * Math.pow(2, (transposedMidi - 69) / 12);

    const osc = ctx.createOscillator();
    const gain = ctx.createGain();
    osc.type = (transposedMidi < 48) ? 'sawtooth' : 'triangle';
    osc.frequency.setValueAtTime(freq, noteStart);

    const targetGain = Math.min(0.22, (n.vel || 0.7) * 0.22);
    gain.gain.setValueAtTime(0.001, noteStart);
    gain.gain.exponentialRampToValueAtTime(targetGain, noteStart + 0.015);
    gain.gain.exponentialRampToValueAtTime(0.001, noteStart + noteDur);

    osc.connect(gain);
    gain.connect(ctx.destination);

    osc.start(noteStart);
    osc.stop(noteStart + noteDur + 0.05);

    _midiSynthActiveNotes.push({ osc, gain, stopTime: noteStart + noteDur });
  });

  function midiStep() {
    if (!state.playing) {
      refreshPlayState();
      return;
    }
    const elapsed = ctx.currentTime - _midiStartTime;
    if (elapsed >= _midiTotalDuration) {
      if (state.loop) {
        playMidiEvents(_currentMidiEvents, _midiTotalDuration);
        return;
      } else {
        refreshPlayState();
        return;
      }
    }

    state.position = Math.min(1.0, elapsed / _midiTotalDuration);
    const timeLbl = $('#timeLabel');
    if (timeLbl) timeLbl.textContent = `${elapsed.toFixed(1)} / ${_midiTotalDuration.toFixed(1)}s`;

    const now = ctx.currentTime;
    let activeCount = 0;
    _currentMidiEvents.forEach((n) => {
      if (now >= _midiStartTime + n.time && now <= _midiStartTime + n.time + n.duration) {
        activeCount++;
      }
    });
    const peak = Math.min(1.0, activeCount * 0.35);
    drawMeterSmoothed(peak);
    drawWaveform();

    _midiPlaybackTimer = requestAnimationFrame(midiStep);
  }
  _midiPlaybackTimer = requestAnimationFrame(midiStep);
}

function extractRootNoteName(keyStr) {
  if (!keyStr || keyStr === 'ORIGINAL' || keyStr === 'UNKNOWN') return 'C';
  const s = String(keyStr).trim();
  const m = s.match(/(?:^|[\s_\-\(\[])([A-Ga-g][#b]?)(?:m|maj|min|minor|major)?(?:[\s_\-\)\]]|$)/i) || s.match(/^([A-Ga-g][#b]?)/i);
  if (!m) return 'C';
  let r = m[1].toUpperCase();
  if (r === 'DB') r = 'C#';
  else if (r === 'EB') r = 'D#';
  else if (r === 'GB') r = 'F#';
  else if (r === 'AB') r = 'G#';
  else if (r === 'BB') r = 'A#';
  return r;
}

function calculateSemitoneDistance(rootNote, targetNote) {
  const rRoot = extractRootNoteName(rootNote);
  const rTarget = extractRootNoteName(targetNote);
  const rootIdx = NOTE_NAMES.indexOf(rRoot);
  const targetIdx = NOTE_NAMES.indexOf(rTarget);
  if (rootIdx < 0 || targetIdx < 0) return 0;
  let diff = targetIdx - rootIdx;
  if (diff > 6) diff -= 12;
  if (diff < -6) diff += 12;
  return diff;
}

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
  let root = extractRootNoteName(rootMatch[1]);
  const rest = rootMatch[2];
  const rootIdx = NOTE_NAMES.indexOf(root);
  if (rootIdx < 0) return (semitones > 0 ? '+' : '') + semitones + 'st';
  const newIdx = ((rootIdx + semitones) % 12 + 12) % 12;
  return NOTE_NAMES[newIdx] + (rest ? ' ' + rest.trim() : '');
}

function extractKeyFromFilename(filename) {
  if (!filename) return null;
  const base = filename.replace(/\.[^.]+$/, '');

  // 1. Camelot (1A..12A, 1B..12B)
  const camelotMatch = base.match(/(?:^|[\s_\-\(\[])([1-9]|1[0-2])([ABab])(?:[\s_\-\)\]]|\.|$)/);
  if (camelotMatch) {
    const num = parseInt(camelotMatch[1], 10);
    const letter = camelotMatch[2].toUpperCase();
    const CAMELOT_TO_KEY = {
      '1B': 'B', '2B': 'F#', '3B': 'C#', '4B': 'G#', '5B': 'D#', '6B': 'A#',
      '7B': 'F', '8B': 'C', '9B': 'G', '10B': 'D', '11B': 'A', '12B': 'E',
      '1A': 'G#m', '2A': 'D#m', '3A': 'A#m', '4A': 'Fm', '5A': 'Cm', '6A': 'Gm',
      '7A': 'Dm', '8A': 'Am', '9A': 'Em', '10A': 'Bm', '11A': 'F#m', '12A': 'C#m'
    };
    const mapped = CAMELOT_TO_KEY[`${num}${letter}`];
    if (mapped) return mapped;
  }

  // 2. Note Name (e.g. D#, C#, E, F, A, G#m, C minor, F# min, etc.)
  const keyRe = /(?:^|[\s_\-\(\[])([A-G][#b]?)(?:\s*(m|maj|min|minor|major))?(?:[\s_\-\)\]]|\.|$)/gi;
  let match;
  const matches = [];
  while ((match = keyRe.exec(base)) !== null) {
    let note = match[1].charAt(0).toUpperCase() + (match[1].length > 1 ? match[1].charAt(1) : '');
    if (note === 'Db' || note === 'DB') note = 'C#';
    else if (note === 'Eb' || note === 'EB') note = 'D#';
    else if (note === 'Gb' || note === 'GB') note = 'F#';
    else if (note === 'Ab' || note === 'AB') note = 'G#';
    else if (note === 'Bb' || note === 'BB') note = 'A#';
    else if (note === 'Cb' || note === 'CB') note = 'B';
    else if (note === 'Fb' || note === 'FB') note = 'E';
    const mode = match[2] ? match[2].toLowerCase() : '';
    const isMin = mode === 'm' || mode.includes('min');
    matches.push(note + (isMin ? 'm' : ''));
  }
  if (matches.length > 0) {
    return matches[matches.length - 1];
  }
  return null;
}

// Pretty-print a sample key for display: "AM" -> "Am", "F#M" -> "F#m",
// "DB" -> "C#", "EB" -> "D#", etc. Preserves an explicit minor "m" suffix
// and normalizes flat spellings to their sharp equivalents (matching the
// 12-note piano keyboard labels).
function normalizeKeyForDisplay(key) {
  if (!key || key === 'ORIGINAL' || key === 'UNKNOWN') return key;
  const m = String(key).match(/^([A-Ga-g])([#bB]?)(.*)$/);
  if (!m) return key;
  let letter = m[1].toUpperCase();
  // Accept both 'b' (lowercase) and 'B' (uppercase) as a flat accidental,
  // since some sources return e.g. "Db" or "DB" for D-flat. A trailing 'B'
  // can't be a mode word here (modes start with m/M/maj/min).
  let accidental = m[2].toLowerCase();
  let mode = m[3].toLowerCase();
  const flatToSharp = { 'Db': 'C#', 'Eb': 'D#', 'Gb': 'F#', 'Ab': 'G#', 'Bb': 'A#' };
  const spelled = letter + accidental;
  if (flatToSharp[spelled]) {
    return flatToSharp[spelled] + (mode.includes('min') || mode === 'm' ? 'm' : '');
  }
  // Normalize mode: anything containing "min"/"m" -> "m"; "maj"/"major" -> "".
  let modeSuffix = '';
  if (mode.includes('min') || mode === 'm') modeSuffix = 'm';
  else if (mode.includes('maj')) modeSuffix = '';
  return letter + accidental + modeSuffix;
}

function updateTransposerPopUI() {
  // 1. Determine canonical key and root note in lockstep
  const canonicalKey = (state.sampleKey && state.sampleKey !== 'ORIGINAL' && state.sampleKey !== 'UNKNOWN')
    ? state.sampleKey
    : (state.originalRootNote || 'C');
  const root = extractRootNoteName(canonicalKey);
  state.originalRootNote = root;
  
  let target = root;
  let semitones = 0;

  // 2. If the user locked a fixed target note (e.g. Tone A), target is ALWAYS that locked note!
  // CRIT-KEY-LOCK: Do NOT recalculate target from pitchSemitones when key is locked.
  // When locked, target is immutable (userTargetNote) and pitchSemitones is the dependent variable.
  if (state.isUserTargetKeyLocked && state.userTargetNote) {
    target = state.userTargetNote;
    semitones = calculateSemitoneDistance(root, state.userTargetNote);
    state.pitchSemitones = semitones;
  } else {
    semitones = state.pitchSemitones || 0;
    if (semitones !== 0) {
      const rootIdx = NOTE_NAMES.indexOf(root);
      if (rootIdx >= 0) {
        target = NOTE_NAMES[((rootIdx + semitones) % 12 + 12) % 12];
      }
    }
  }
  state.selectedTargetNote = target;

  // 3. Display string
  const displayRoot = normalizeKeyForDisplay(canonicalKey);

  const rootBadge = $('#pianoRootBadge');
  if (rootBadge) {
    if (root === target || semitones === 0) {
      rootBadge.textContent = `Root: ${displayRoot}`;
    } else {
      const modeMatch = displayRoot.match(/m$/);
      rootBadge.textContent = `Root: ${displayRoot} ➔ ${target}${modeMatch ? 'm' : ''}`;
    }
  }

  const semitoneLabel = $('#pianoSemitoneLabel');
  if (semitoneLabel) {
    if (semitones === 0) {
      semitoneLabel.textContent = `0 semitones`;
    } else {
      semitoneLabel.textContent = `${semitones > 0 ? '+' : ''}${semitones} semitones`;
    }
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
    if (semitones === 0) {
      keyLabel.textContent = `KEY: ${displayRoot}`;
    } else {
      const modeMatch = displayRoot.match(/m$/);
      keyLabel.textContent = `KEY: ${target}${modeMatch ? 'm' : ''}`;
    }
  }

  const resetBtn = $('#btnResetKey');
  if (resetBtn) {
    resetBtn.textContent = `${tr('player.originalKey') || 'Original Key'} (${displayRoot})`;
  }

  $$('#pianoKeyboard .piano-key').forEach((k) => {
    const note = k.dataset.note;
    const isTarget = (note === target);
    const isRoot = (note === root && root !== target);
    k.classList.toggle('active', isTarget);
    k.classList.toggle('root-marker', isRoot);
  });
}

async function setTargetNote(targetNote) {
  const root = state.originalRootNote || 'C';
  const validTarget = extractRootNoteName(targetNote);
  state.isUserTargetKeyLocked = true;
  state.userTargetNote = validTarget;
  state.selectedTargetNote = validTarget;
  const semitones = calculateSemitoneDistance(root, validTarget);
  await setPitchShift(semitones);
  playSynthNote(validTarget, 0.3);
}

async function setPitchShift(semitones) {
  state.pitchSemitones = semitones;
  const root = state.originalRootNote || 'C';
  const rootIdx = NOTE_NAMES.indexOf(root);
  const targetIdx = (rootIdx + semitones + 120) % 12;
  state.selectedTargetNote = NOTE_NAMES[targetIdx];

  updateTransposerPopUI();

  if (state.selected && isMidiFile(state.selected) && state.playing && state.midiNotes) {
    playMidiEvents(state.midiNotes, state.duration);
  } else {
    try {
      await bridge('audio.setPitchShift', { semitones });
    } catch (err) {
      console.error('setPitchShift failed:', err);
    }
  }
  drawWaveform();
}

async function resetOriginalKey() {
  state.isUserTargetKeyLocked = false;
  state.userTargetNote = null;
  state.selectedTargetNote = state.originalRootNote || 'C';
  await setPitchShift(0);
  playSynthNote(state.selectedTargetNote, 0.3);
}

async function toggleSyncBpm() {
  state.syncBpm = !state.syncBpm;
  localStorage.setItem('reals_sync_bpm', state.syncBpm ? 'true' : 'false');
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
  audioLab: '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"><line x1="6" y1="4" x2="6" y2="20"/><line x1="12" y1="4" x2="12" y2="20"/><line x1="18" y1="4" x2="18" y2="20"/><circle cx="6" cy="9" r="2.2" fill="var(--bg-app)"/><circle cx="12" cy="15" r="2.2" fill="var(--bg-app)"/><circle cx="18" cy="8" r="2.2" fill="var(--bg-app)"/></svg>',
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
  state.isDocked = state.docked;
  document.documentElement.classList.toggle('docked', state.docked);
  document.body.classList.toggle('docked', state.docked);
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
  // Fast 0ms inline bootstrap from localStorage before async bridge
  const cachedTreeW = localStorage.getItem('reals_tree_width');
  if (cachedTreeW && $('#tree')) $('#tree').style.width = cachedTreeW + 'px';
  const cachedPrevH = localStorage.getItem('reals_preview_height');
  if (cachedPrevH && $('#preview')) $('#preview').style.height = cachedPrevH + 'px';
  const cachedTreeCol = localStorage.getItem('reals_tree_collapsed');
  if (cachedTreeCol === 'true' && $('#tree')) {
    $('#tree').classList.add('collapsed');
    $('#btnToggleTree')?.classList.remove('on');
  }
  const cachedVol = localStorage.getItem('reals_volume');
  if (cachedVol !== null && cachedVol !== '0.9') {
    state.volume = parseFloat(cachedVol);
  } else {
    state.volume = 1.0;
    localStorage.setItem('reals_volume', '1.0');
  }
  const volEl = $('#volume');
  if (volEl) volEl.value = state.volume;
  bridge('audio.setVolume', { value: state.volume }).catch(() => {});
  const cachedLoop = localStorage.getItem('reals_loop');
  if (cachedLoop !== null) {
    state.loop = cachedLoop === 'true';
    $('#btnLoop')?.classList.toggle('on', state.loop);
    bridge('audio.setLoop', { value: state.loop }).catch(() => {});
  }
  const cachedSyncBpm = localStorage.getItem('reals_sync_bpm');
  if (cachedSyncBpm !== null) {
    state.syncBpm = cachedSyncBpm === 'true';
    $('#btnSyncBpm')?.classList.toggle('on', state.syncBpm);
    // Sync backend state on startup so m_impl->syncEnabled matches UI
    if (state.syncBpm) {
      bridge('audio.setSyncBpm', { enabled: true, bpm: 120.0, sampleBpm: 0, path: '' }).catch(() => {});
    }
  }
  const cachedExp = localStorage.getItem('reals_expanded');
  if (cachedExp) {
    try {
      const arr = JSON.parse(cachedExp);
      if (Array.isArray(arr)) state.expanded = new Set(arr.map(normPath));
    } catch {}
  }

  try {
    const cfg = await bridge('config.getAll');
    if (cfg) {
      LANG = cfg.language || 'vi';
      const savedTheme = cfg.theme || localStorage.getItem('reals_theme') || 'dark-studio';
      if (window.themeManager) {
        window.themeManager.applyTheme(savedTheme, false);
      }
      if (savedTheme === 'dark-studio') {
        applyAccent(cfg.accent || 'orange');
      }
      applyNoise(cfg.noiseOverlay !== false);
      state.autoCollapseTree = cfg.autoCollapseTree === true;
      applyDisplaySize(cfg.displaySize || 'medium');
      applyNavPosition(cfg.navPosition || 'top');
      applyMiniWaveSetting(localStorage.getItem('reals_mini_wave') === 'true');
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
  initSettingsModal();
}

function openSettingsModal() {
  const modal = $('#settingsModal');
  if (!modal) return;
  modal.classList.remove('hidden');
  renderSettingsModal();
}

function closeSettingsModal() {
  const modal = $('#settingsModal');
  if (modal) modal.classList.add('hidden');
}

function initSettingsModal() {
  const btnSettings = $('#btnSettings');
  if (btnSettings) {
    btnSettings.onclick = (e) => {
      e.stopPropagation();
      openSettingsModal();
    };
  }
  const btnClose = $('#btnSettingsClose');
  if (btnClose) {
    btnClose.onclick = (e) => {
      e.stopPropagation();
      closeSettingsModal();
    };
  }
  const modal = $('#settingsModal');
  if (modal) {
    modal.onclick = (e) => {
      if (e.target === modal) closeSettingsModal();
    };
  }

  // Tab switching
  $$('.settings-tab-btn').forEach((btn) => {
    btn.onclick = (e) => {
      e.stopPropagation();
      const tabName = btn.dataset.tab;
      $$('.settings-tab-btn').forEach((b) => b.classList.toggle('active', b === btn));
      $$('.settings-tab-pane').forEach((p) => p.classList.toggle('active', p.id === `tab-${tabName}`));
    };
  });
}

function renderSettingsModal() {
  bridge('config.getAll').then((cfg) => {
    cfg = cfg || {};
    // Tab 1: General
    $$('#optDisplaySize .setting-chip').forEach((c) => {
      c.classList.toggle('active', c.dataset.val === state.displaySize);
      c.onclick = (e) => {
        e.stopPropagation();
        const v = c.dataset.val;
        bridge('config.set', { key: 'displaySize', value: v });
        applyDisplaySize(v);
        renderSettingsModal();
      };
    });
    $$('#optNavPos .setting-chip').forEach((c) => {
      const curNav = state.navPosition || cfg.navPosition || 'top';
      c.classList.toggle('active', c.dataset.val === curNav);
      c.onclick = (e) => {
        e.stopPropagation();
        const v = c.dataset.val;
        state.navPosition = v;
        bridge('config.set', { key: 'navPosition', value: v });
        applyNavPosition(v);
        renderSettingsModal();
      };
    });
    const curTheme = (window.themeManager && window.themeManager.getTheme()) || 'dark-studio';
    $$('#optTheme .setting-chip').forEach((c) => {
      c.classList.toggle('active', c.dataset.val === curTheme);
      c.onclick = (e) => {
        e.stopPropagation();
        const v = c.dataset.val;
        if (window.themeManager) {
          window.themeManager.applyTheme(v, true);
        }
        renderSettingsModal();
      };
    });

    // Tab 2: File Browser
    const showMiniWave = localStorage.getItem('reals_mini_wave') === 'true'; // default false
    $$('#optMiniWave .setting-chip').forEach((c) => {
      c.classList.toggle('active', c.dataset.val === (showMiniWave ? 'true' : 'false'));
      c.onclick = (e) => {
        e.stopPropagation();
        const v = c.dataset.val === 'true';
        localStorage.setItem('reals_mini_wave', v ? 'true' : 'false');
        applyMiniWaveSetting(v);
        renderSettingsModal();
      };
    });

    const autoCol = state.autoCollapseTree === true && cfg.autoCollapseTree === true;
    $$('#optAutoCollapse .setting-chip').forEach((c) => {
      c.classList.toggle('active', c.dataset.val === (autoCol ? 'true' : 'false'));
      c.onclick = (e) => {
        e.stopPropagation();
        const v = c.dataset.val === 'true';
        state.autoCollapseTree = v;
        bridge('config.set', { key: 'autoCollapseTree', value: v });
        if (v && state.currentDir) {
          tidyExpandedFolders(state.currentDir);
          renderTree();
        }
        renderSettingsModal();
      };
    });

    const curCpu = state.scannerCpuMode || cfg.scannerCpuMode || 'normal';
    $$('#optCpuMode .setting-chip').forEach((c) => {
      c.classList.toggle('active', c.dataset.val === curCpu);
      c.onclick = (e) => {
        e.stopPropagation();
        const v = c.dataset.val;
        if (v === 'high' && !confirm(tr('scanner.cpuMode.highWarn') || 'High CPU mode may impact DAW performance. Continue?')) return;
        state.scannerCpuMode = v;
        bridge('scanner.setCpuMode', { cpuMode: v });
        bridge('config.set', { key: 'scannerCpuMode', value: v });
        const scSelect = $('#scannerCpuMode');
        if (scSelect) scSelect.value = v;
        renderSettingsModal();
      };
    });
  });
}

function applyMiniWaveSetting(show) {
  if (show) {
    document.body.classList.remove('hide-mini-wave');
  } else {
    document.body.classList.add('hide-mini-wave');
  }
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

// ============ Browser ============
function getRowH() {
  if (state.displaySize === 'small') return 36;
  if (state.displaySize === 'large') return 56;
  return 46;
}
const VIRT_OVERSCAN = 8;

async function initBrowser() {
  const rawRoots = await bridge('fs.roots');
  state.roots = (rawRoots || []).map((r) => ({ name: r.name, path: normPath(r.path) }));
  if (state.roots.length) state.currentDir = state.roots[0].path;
  // Restore the last visited directory (saved on every openDir call) so the
  // user lands back where they left off instead of always on the first root.
  try {
    const saved = localStorage.getItem('reals_last_dir');
    if (saved && state.roots.some((r) => isPathUnder(saved, r.path))) {
      state.currentDir = normPath(saved);
      expandPathAncestors(state.currentDir);
    }
  } catch {}
  try {
    const t = await bridge('browser.tags');
    state.tagCache = (t && t.tags) || {};
  } catch { state.tagCache = {}; }
  renderTree();
  wireBrowserEvents();
  if (state.currentDir) openDir(state.currentDir);
}

async function refreshRoots() {
  const rawRoots = await bridge('fs.roots');
  state.roots = (rawRoots || []).map((r) => ({ name: r.name, path: normPath(r.path) }));
}

// Removed dead function renderRoots(): the <select id="roots"> it targeted
// doesn't exist in index.html, so every call was an early `return`. The
// folder tree (#treeNodes / #tree) is the only roots surface now.

async function subdirsOf(path) {
  const np = normPath(path);
  if (state.subCache[np]) return state.subCache[np];
  const subs = await bridge('fs.subdirs', { path: np });
  state.subCache[np] = subs || [];
  return state.subCache[np];
}

function isNarrowTreeMode() {
  return window.innerWidth <= 360;
}

function buildTreeSync(frag, roots, depth = 0) {
  const narrow = isNarrowTreeMode();
  for (const r of roots) {
    const np = normPath(r.path);
    const isExpanded = state.expanded.has(np);
    frag.appendChild(folderRowEl(np, r.name, depth));
    if (isExpanded) {
      const subs = state.subCache[np];
      if (subs && subs.length) {
        const childRoots = subs.map((s) => ({ name: s, path: joinPath(np, s) }));
        buildTreeSync(frag, childRoots, depth + 1);
      }
      if (narrow) {
        state.treeFilesCache = state.treeFilesCache || {};
        const files = state.treeFilesCache[np];
        if (files && files.length) {
          const limit = Math.min(files.length, 120);
          for (let fi = 0; fi < limit; ++fi) {
            frag.appendChild(sideFileRow(files[fi], depth + 1));
          }
          if (files.length > limit) {
            const more = el('div', 'tree-row muted', `+ ${files.length - limit} more files...`);
            more.style.paddingLeft = 14 + (depth + 1) * 12 + 'px';
            frag.appendChild(more);
          }
        }
      }
    }
  }
}

async function renderTree() {
  const tree = $('#tree');
  if (!tree) return;
  const seq = ++state.treeSeq;

  if (!state.favSet) {
    try {
      const favs = await bridge('browser.favorites');
      if (seq !== state.treeSeq) return;
      state.favSet = new Set(favs || []);
    } catch {
      state.favSet = new Set();
    }
  }

  // 1. FAST SYNCHRONOUS PASS: Update DOM immediately (0ms response!)
  const container = $('#treeNodes') || tree;
  const currentScroll = tree.scrollTop;
  const frag = document.createDocumentFragment();
  buildTreeSync(frag, state.roots, 0);
  container.replaceChildren(frag);
  tree.scrollTop = currentScroll;

  // 2. PARALLEL BACKGROUND PASS: If any expanded folder is not in cache, fetch in parallel
  const uncached = Array.from(state.expanded).map(normPath).filter((p) => !state.subCache[p]);
  if (uncached.length > 0) {
    await Promise.all(uncached.map(async (p) => {
      try {
        const subs = await bridge('fs.subdirs', { path: p });
        state.subCache[p] = subs || [];
      } catch {
        state.subCache[p] = [];
      }
    }));
    if (seq !== state.treeSeq) return;

    // Synchronous re-render once all subdirs are populated
    const freshFrag = document.createDocumentFragment();
    buildTreeSync(freshFrag, state.roots, 0);
    const s = tree.scrollTop;
    container.replaceChildren(freshFrag);
    tree.scrollTop = s;
  }

  // 3. FL STUDIO NARROW MODE: Fetch files for expanded folders
  if (isNarrowTreeMode()) {
    state.treeFilesCache = state.treeFilesCache || {};
    const uncachedFiles = Array.from(state.expanded).map(normPath).filter((p) => !state.treeFilesCache[p]);
    if (uncachedFiles.length > 0) {
      await Promise.all(uncachedFiles.map(async (p) => {
        try {
          const res = await bridge('fs.list', { path: p });
          const raw = Array.isArray(res) ? res : ((res && res.files) || []);
          state.treeFilesCache[p] = raw.filter((f) => !f.isDir);
        } catch {
          state.treeFilesCache[p] = [];
        }
      }));
      if (seq !== state.treeSeq) return;
      const freshFrag2 = document.createDocumentFragment();
      buildTreeSync(freshFrag2, state.roots, 0);
      const s2 = tree.scrollTop;
      container.replaceChildren(freshFrag2);
      tree.scrollTop = s2;
    }
  }
}

function saveExpandedFolders() {
  if (state.expanded) {
    try {
      localStorage.setItem('reals_expanded', JSON.stringify(Array.from(state.expanded).map(normPath)));
    } catch {}
  }
}

function expandPathAncestors(targetPath) {
  if (!targetPath || !state.roots) return;
  const nTarget = normPath(targetPath);
  for (const r of state.roots) {
    const nRoot = normPath(r.path);
    if (isPathUnder(nTarget, nRoot)) {
      state.expanded.add(nRoot);
      if (nTarget.toLowerCase() !== nRoot.toLowerCase()) {
        const rel = nTarget.slice(nRoot.length);
        const parts = rel.split('\\').filter(Boolean);
        let curr = nRoot;
        for (const part of parts) {
          curr += '\\' + part;
          state.expanded.add(curr);
        }
      }
      break;
    }
  }
}

function tidyExpandedFolders(activePath) {
  if (!state.autoCollapseTree || !activePath) return;
  const nActive = normPath(activePath);
  const needed = new Set();
  for (const exp of state.expanded) {
    const nExp = normPath(exp);
    if (isPathUnder(nActive, nExp)) {
      needed.add(nExp);
    }
  }
  needed.add(nActive);
  state.expanded = needed;
}

function folderRowEl(path, name, depth) {
  const narrow = isNarrowTreeMode();
  const nPath = normPath(path);
  const row = el('div', 'tree-row' + (isSamePath(state.currentDir, nPath) ? ' on' : ''));
  const basePad = narrow ? 3 : 10;
  const stepPad = narrow ? 6 : 12;
  row.style.paddingLeft = basePad + depth * stepPad + 'px';
  row.title = name;

  const isExp = state.expanded.has(nPath);
  const twist = el('span', 'twist', isExp ? '▼' : '▶');
  twist.onclick = (e) => {
    e.preventDefault();
    e.stopPropagation();
    if (state.expanded.has(nPath)) {
      state.expanded.delete(nPath);
      const prefix = nPath.toLowerCase() + '\\';
      for (const exp of Array.from(state.expanded)) {
        if (normPath(exp).toLowerCase().startsWith(prefix)) state.expanded.delete(exp);
      }
    } else {
      if (state.autoCollapseTree) {
        tidyExpandedFolders(nPath);
      } else {
        state.expanded.add(nPath);
      }
    }
    saveExpandedFolders();
    renderTree();
  };
  row.appendChild(twist);

  const nameSpan = el('span', 'name');
  if (name.length > 10) {
    const tailLen = Math.max(4, Math.min(8, Math.floor(name.length * 0.3)));
    nameSpan.appendChild(el('span', 'name-start', name.slice(0, name.length - tailLen)));
    nameSpan.appendChild(el('span', 'name-end', name.slice(name.length - tailLen)));
  } else {
    nameSpan.textContent = name;
  }
  row.appendChild(nameSpan);

  row.onclick = (e) => {
    e.preventDefault();
    // Clicking folder name/row selects and opens folder.
    // If not expanded, expand it. If already expanded, DO NOT collapse it!
    if (!state.expanded.has(nPath)) {
      if (state.autoCollapseTree) {
        tidyExpandedFolders(nPath);
      } else {
        state.expanded.add(nPath);
      }
    }
    saveExpandedFolders();
    openDir(nPath);
    renderTree();
  };
  row.oncontextmenu = (e) => { e.preventDefault(); folderMenu(e, nPath); };
  return row;
}

function sideFileRow(f, depth = 0) {
  const narrow = isNarrowTreeMode();
  const row = el('div', 'tree-row side-file' + (state.selected === f.path ? ' on' : ''));
  const basePad = narrow ? 6 : 14;
  const stepPad = narrow ? 6 : 12;
  row.style.paddingLeft = basePad + depth * stepPad + 'px';
  row.title = f.path || f.name;

  if (state.favSet && state.favSet.has(f.path)) {
    row.appendChild(el('span', 'star', '★'));
  } else {
    const icon = isMidiFile(f) ? '🎹' : '♪';
    row.appendChild(el('span', 'tree-file-icon', icon));
  }

  const nameSpan = el('span', 'name');
  if (f.name.length > 10) {
    const tailLen = Math.max(5, Math.min(10, Math.floor(f.name.length * 0.35)));
    nameSpan.appendChild(el('span', 'name-start', f.name.slice(0, f.name.length - tailLen)));
    nameSpan.appendChild(el('span', 'name-end', f.name.slice(f.name.length - tailLen)));
  } else {
    nameSpan.textContent = f.name;
  }
  row.appendChild(nameSpan);

  row.onclick = (e) => {
    e.stopPropagation();
    selectEntry(f);
  };
  row.oncontextmenu = (e) => { e.preventDefault(); e.stopPropagation(); fileMenu(e, f); };
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
function armOleDrag(row, fileOrGetter) {
  if (!row) return;
  row.draggable = false;
  row.addEventListener('pointerdown', (e) => {
    if (e.button !== 0) return;
    const f = typeof fileOrGetter === 'function' ? fileOrGetter() : fileOrGetter;
    if (!f || f.isDir || !f.path) return;
    disarmOleDrag();
    const startX = e.clientX;
    const startY = e.clientY;
    let didDrag = false;
    const move = (ev) => {
      const dx = ev.clientX - startX;
      const dy = ev.clientY - startY;
      if (dx * dx + dy * dy < DRAG_THRESH * DRAG_THRESH) return;
      disarmOleDrag();
      didDrag = true;
      state._suppressClick = true;
      setTimeout(() => { state._suppressClick = false; }, 300);
      if (hasWebView) {
        let currentPitch = state.pitchSemitones || 0;
        if (state.isUserTargetKeyLocked && state.userTargetNote) {
          const fileRoot = extractRootNoteName(f.key || extractKeyFromFilename(f.name || f.path) || (state.selected === f.path ? state.originalRootNote : 'C'));
          currentPitch = calculateSemitoneDistance(fileRoot, state.userTargetNote);
        }
        bridge('browser.beginDrag', {
          path: f.path,
          syncBpm: !!state.syncBpm,
          sampleBpm: f.bpm || (state.selected === f.path ? state.sampleBpm : 0) || 0,
          pitchSemitones: currentPitch
        }).catch(() => {});
      }
    };
    const up = () => {
      disarmOleDrag();
      if (didDrag) {
        setTimeout(() => { state._suppressClick = false; }, 50);
      }
    };
    state._dragArm = { move, up };
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
    window.addEventListener('pointercancel', up);
  });
}

function generateMiniWaveSvg(f) {
  const p = typeof f === 'string' ? f : (f ? f.path : '');
  if (!p) return '';
  let bars = state.envCache && state.envCache[p];
  if (!bars || !bars.length) {
    if (state.selected === p && state.envelope && state.envelope.length) {
      bars = state.envelope;
    }
  }
  // Only render REAL audio waveforms — never render fake placeholder sine waves!
  if (!bars || !bars.length) {
    return '';
  }

  const count = 84;
  let rects = '';
  const barW = 280 / count;
  const drawW = 1.4;
  const mid = 11;
  const maxAmp = 10.2;

  for (let i = 0; i < count; ++i) {
    const idx = Math.floor((i / count) * bars.length);
    const rawVal = bars[idx] || 0.01;
    const val = Math.min(1.0, Math.max(0.02, rawVal));
    const h = Math.max(0.8, Math.pow(val, 0.75) * maxAmp);
    const x = (i * barW).toFixed(1);
    const y = (mid - h).toFixed(1);
    const barH = (h * 2).toFixed(1);
    rects += `<rect x="${x}" y="${y}" width="${drawW}" height="${barH}" rx="0.7" fill="currentColor"/>`;
  }
  return `<svg viewBox="0 0 280 22" preserveAspectRatio="none" class="mini-preview-svg">${rects}</svg>`;
}

function updateRowMiniWave(path) {
  if (!path) return;
  const spacer = $('#fileSpacer');
  if (!spacer) return;
  const row = spacer.querySelector(`.file-row[title="${CSS.escape(path)}"]`);
  if (!row) return;
  let bg = row.querySelector('.mini-preview-bg');
  const svg = generateMiniWaveSvg(path);
  if (!svg) {
    if (bg) bg.innerHTML = '';
    return;
  }
  if (!bg) {
    bg = el('div', 'mini-preview-bg');
    row.insertBefore(bg, row.firstChild);
  }
  bg.innerHTML = svg;
}

function generateMiniMidiSvg(f) {
  const p = typeof f === 'string' ? f : (f ? f.path : '');
  let pathHash = 0;
  for (let i = 0; i < p.length; ++i) {
    pathHash = (Math.imul(31, pathHash) + p.charCodeAt(i)) | 0;
  }
  let rects = '';
  const numNotes = 12;
  const colors = ['#55A5FF', '#35D07F', '#B98CFF', '#55A5FF'];
  for (let i = 0; i < numNotes; ++i) {
    const x = ((i * 23 + ((pathHash >> (i % 8)) & 15)) % 255);
    const w = 12 + ((pathHash >> ((i + 2) % 8)) & 18);
    const row = ((pathHash >> ((i + 1) % 6)) & 3);
    const y = 3 + row * 4.5;
    const col = colors[row % colors.length];
    rects += `<rect x="${x}" y="${y}" width="${w}" height="2.5" rx="1" fill="${col}" opacity="0.45"/>`;
  }
  return `<svg viewBox="0 0 280 22" preserveAspectRatio="none" class="mini-preview-svg">${rects}</svg>`;
}

function openDir(path) {
  if (!path) return;
  const nPath = normPath(path);
  const box = $('#files');
  if (box && state.currentDir && !isSamePath(state.currentDir, nPath)) {
    state.dirScrolls[state.currentDir] = box.scrollTop;
  }
  state.currentDir = nPath;
  localStorage.setItem('reals_last_dir', nPath);
  expandPathAncestors(nPath);
  saveExpandedFolders();
  state.similarSource = null;
  state.similarSourceName = null;
  state.searchQ = '';
  state.searchPending = false;
  state.searchGen = ++state.searchSeq;
  if ($('#search')) $('#search').value = '';
  // Reset favorites-only mode when navigating to a folder — otherwise the
  // list stays filtered by the previous folder's favorites, leaving the
  // user with a stale/empty view that doesn't match the current dir.
  if (state.favOnly) {
    state.favOnly = false;
    state.savedDirBeforeFav = null;
    const favBtn = $('#favOnly');
    if (favBtn) favBtn.classList.remove('on');
  }
  bridge('fs.watch', { path }).catch(() => {});
  loadDir(path, false);
}

function paintLoadingFiles() {
  const header = $('#filesHead');
  if (header) header.textContent = state.currentDir || tr('browser.pickRoot');
  const box = $('#files');
  if (!box) return;

  const loader = el('div', 'files-loader');
  const spinner = el('div', 'loading-spinner');
  const text = el('div', 'loading-text', tr('browser.loadingSamples'));
  loader.appendChild(spinner);
  loader.appendChild(text);
  box.replaceChildren(loader);
}

function loadDir(path, force) {
  if (!path) return;
  const reqId = ++state.listSeq;
  paintLoadingFiles();
  const go = () => bridge('fs.list', { path, sort: state.sort }).then((files) => {
    if (reqId !== state.listSeq) return;
    const raw = Array.isArray(files) ? files : ((files && files.files) || []);
    state.rawFiles = raw;
    state.treeFilesCache = state.treeFilesCache || {};
    state.treeFilesCache[path] = raw.filter((f) => !f.isDir);
    const isReloadingCurrent = (state.listDir === path);
    state.listDir = path;
    paintFromRaw(isReloadingCurrent);
    probeVisibleAudio();
    if (isNarrowTreeMode() && state.expanded && state.expanded.has(path)) {
      renderTree();
    }
  }).catch(() => {
    if (reqId !== state.listSeq) return;
    state.rawFiles = [];
    paintFromRaw();
  });
  if (force) bridge('fs.invalidate', { path }).then(go, go);
  else go();
}

function filteredFiles() {
  const out = state.rawFiles.filter((f) => {
    if (f.isDir) return false;
    if (f.name && (f.name.toLowerCase() === 'desktop.ini' || f.name.toLowerCase() === 'thumbs.db' || f.name === '.DS_Store')) return false;
    if (state.audioOnly && !f.isAudio) return false;
    if (state.favOnly && !state.favSet.has(f.path)) return false;
    if (state.tagFilter > 0 && (state.tagCache[f.path] || 0) !== state.tagFilter) return false;
    return true;
  });
  // Sort the filtered result so the Sort dropdown actually takes effect in
  // search / favorites / tag-filter modes too (previously only the bridge
  // reload on plain dir mode applied sorting).
  sortFileList(out);
  return out;
}

// Shared comparator for the Sort dropdown (0 = name, 1 = size, 2 = date).
// Used both when reloading a dir (via bridge) and when filtering in-memory so
// Sort works consistently across all modes.
function sortFileList(list) {
  if (!Array.isArray(list) || list.length < 2) return;
  const coll = (typeof Intl !== 'undefined' && Intl.Collator)
    ? new Intl.Collator(undefined, { numeric: true, sensitivity: 'base' })
    : null;
  const cmp = coll ? coll.compare : (a, b) => String(a).localeCompare(String(b));
  list.sort((a, b) => {
    if (state.sort === 1) {
      const sa = (a.size !== undefined ? a.size : (a.filesize || 0)) || 0;
      const sb = (b.size !== undefined ? b.size : (b.filesize || 0)) || 0;
      return sb - sa; // largest first
    }
    if (state.sort === 2) {
      const ta = (a.modified !== undefined ? a.modified : (a.mtime || 0)) || 0;
      const tb = (b.modified !== undefined ? b.modified : (b.mtime || 0)) || 0;
      return tb - ta; // newest first
    }
    // default: name (0)
    return cmp(a.name || '', b.name || '');
  });
}

function paintFromRaw(preserveScroll = false) {
  if (state.similarSource) {
    state.files = (state.rawFiles || []).slice();
    sortFileList(state.files);
  } else {
    state.files = filteredFiles();
  }
  const box = $('#files');
  if (!box) return;

  const loader = box.querySelector('.files-loader');
  if (loader) loader.remove();

  const savedScroll = preserveScroll ? box.scrollTop : (state.searchQ ? 0 : (state.dirScrolls[state.currentDir] || 0));

  const header = $('#filesHead') || box.querySelector('.files-head');
  if (header) {
    const q = (state.searchQ || '').trim();
    if (state.similarSource) {
      header.textContent = `${tr('browser.similarTo')}: ${state.similarSourceName || ''} (${state.files.length})`;
    } else if (state.favOnly) {
      header.textContent = `★ ${tr('browser.favOnly')} (${state.files.length})`;
    } else {
      header.textContent = q
        ? (state.searchPending ? tr('browser.searching') : `${tr('browser.results')}: ${state.files.length}`)
        : (state.currentDir || tr('browser.pickRoot'));
    }
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
  const total = files.length;
  const rowH = getRowH();
  spacer.style.height = Math.max(rowH, total * rowH) + 'px';
  const scroll = box.scrollTop;
  const viewH = box.clientHeight || 300;
  let start = Math.max(0, Math.floor(scroll / rowH) - VIRT_OVERSCAN);
  let end = Math.min(total, Math.ceil((scroll + viewH) / rowH) + VIRT_OVERSCAN);
  if (total <= 80) { start = 0; end = total; }
  spacer.replaceChildren();
  for (let i = start; i < end; ++i) {
    const row = fileRowEl(files[i], state.selected === files[i].path, false);
    row.style.position = 'absolute';
    row.style.left = '4px';
    row.style.right = '4px';
    row.style.top = (i * rowH) + 'px';
    row.style.height = rowH + 'px';
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
  if (!f) return el('div');
  const rawName = f.name || f.filename || (f.path ? f.path.split(/[\\/]/).pop() : '') || '';
  if (!f.name) f.name = rawName;
  if (!f.filename) f.filename = rawName;
  const isDir = !!f.isDir;
  const row = el('div', 'file-row' + (isSelected ? ' sel' : '') + (isDir ? ' dir' : ''));
  row._path = f.path;
  if (f.path) row.title = f.path;

  // Mini preview background (Waveform for audio, Piano roll for MIDI)
  if (!isDir) {
    const bg = el('div', 'mini-preview-bg');
    if (isMidiFile(f)) {
      bg.innerHTML = generateMiniMidiSvg(f);
    } else {
      bg.innerHTML = generateMiniWaveSvg(f);
    }
    row.appendChild(bg);
  }

  // Favorite Star right at the START
  if (state.favSet && state.favSet.has(f.path)) {
    row.appendChild(el('span', 'star', '★'));
  }

  const tag = (state.tagCache && state.tagCache[f.path]) || 0;
  if (tag > 0) {
    const d = el('span', 'tagdot');
    d.style.background = TAG_COLORS[tag];
    row.appendChild(d);
  } else if (!state.favSet || !state.favSet.has(f.path)) {
    row.appendChild(el('span', 'tagdot'));
  }
  const label = isDir ? ('▸ ' + rawName) : rawName;
  const fnameSpan = el('span', 'fname');
  const textSpan = el('span', 'fname-text');
  if (label && label.length > 14) {
    const tailLen = Math.max(7, Math.min(14, Math.floor(label.length * 0.35)));
    textSpan.appendChild(el('span', 'fname-start', label.slice(0, label.length - tailLen)));
    textSpan.appendChild(el('span', 'fname-end', label.slice(label.length - tailLen)));
  } else {
    textSpan.textContent = label || '';
  }
  fnameSpan.appendChild(textSpan);
  row.appendChild(fnameSpan);
  if (!compact && !isDir) {
    if (isMidiFile(f)) {
      row.appendChild(el('span', 'fmeta-badge midi', 'MIDI'));
    }
    const simVal = (f.similarity !== undefined && f.similarity > 0) ? f.similarity : (f.score !== undefined && f.score > 0 ? Math.round(f.score * 100) : (state.similarSource ? 95 : null));
    if (simVal && (state.similarSource || f.similarity || f.score)) {
      row.appendChild(el('span', 'fmeta-badge sim-badge', `${simVal}%`));
    }
    if (f.duration && !state.probeCache[f.path]) state.probeCache[f.path] = f.duration;
    const dur = state.probeCache[f.path] || f.duration;
    if (dur) row.appendChild(el('span', 'fdur', fmtDur(dur)));
    if (f.bpm && f.bpm > 0) {
      row.appendChild(el('span', 'fmeta-badge', Math.round(f.bpm) + ' BPM'));
    }
    if (f.key) {
      row.appendChild(el('span', 'fmeta-badge', f.key));
    }
    const sz = f.size !== undefined ? f.size : f.filesize;
    if (sz !== undefined) row.appendChild(el('span', 'fsize', fmtSize(sz)));
  }
  row.onclick = (e) => {
    if (state._suppressClick) { e.preventDefault(); e.stopPropagation(); return; }
    e.preventDefault();
    e.stopPropagation(); // prevent bubbling to filesBox.onclick, which would
    // call selectEntry a second time (double-call waste: UI re-render,
    // transposer update, and play-arm timer each run twice).
    selectEntry(f);
  };
  row.ondblclick = () => {
    if (f.isDir) openDir(f.path);
  };
  row.oncontextmenu = (e) => { e.preventDefault(); fileMenu(e, f); };
  armOleDrag(row, f);
  return row;
}

function selectEntry(f) {
  if (!f) return;
  const now = Date.now();
  const isDuplicate = state._lastTriggerPath === f.path && (now - (state._lastTriggerTime || 0) < 250);
  state._lastTriggerPath = f.path;
  state._lastTriggerTime = now;

  state.selected = f.path;
  const spacer = $('#fileSpacer');
  if (spacer) {
    const prev = spacer.querySelector('.file-row.sel');
    if (prev && prev._path !== f.path) prev.classList.remove('sel');
    const cur = spacer.querySelector(`.file-row[title="${CSS.escape(f.path)}"]`);
    if (cur) cur.classList.add('sel');
  }
  const treeNodes = $('#treeNodes') || $('#tree');
  if (treeNodes) {
    const prevTree = treeNodes.querySelector('.tree-row.side-file.on');
    if (prevTree && prevTree.title !== f.path) prevTree.classList.remove('on');
    const curTree = treeNodes.querySelector(`.tree-row.side-file[title="${CSS.escape(f.path)}"]`);
    if (curTree) curTree.classList.add('on');
  }
  if (!f.isDir) {
    const filename = f.path.split(/[\\/]/).pop() || '';
    const extractedKey = (f.key) || extractKeyFromFilename(filename) || 'C';
    const rootNote = extractRootNoteName(extractedKey);
    state.sampleKey = extractedKey;
    state.originalRootNote = rootNote;

    if (state.isUserTargetKeyLocked && state.userTargetNote) {
      state.selectedTargetNote = state.userTargetNote;
      state.pitchSemitones = calculateSemitoneDistance(rootNote, state.userTargetNote);
    } else {
      state.selectedTargetNote = rootNote;
      state.pitchSemitones = 0;
    }
    updateTransposerPopUI();

    if (isMidiFile(f.path)) {
      loadMidiPreviewData(f.path).then((parsed) => {
        if (parsed && state.selected === f.path) {
          state.midiNotes = parsed.notes;
          state.duration = parsed.duration;
          state.originalRootNote = parsed.tonic || rootNote;
          if (state.isUserTargetKeyLocked && state.userTargetNote) {
            state.selectedTargetNote = state.userTargetNote;
            state.pitchSemitones = calculateSemitoneDistance(state.originalRootNote, state.userTargetNote);
          } else {
            state.selectedTargetNote = state.originalRootNote;
            state.pitchSemitones = 0;
          }
          updateTransposerPopUI();
          drawWaveform();
        }
      });
    }

    if (!isDuplicate) {
      if (state.autoPreview !== false) {
        clearTimeout(state._previewArmT);
        state._previewArmT = setTimeout(() => {
          if (state.selected === f.path) playFile(f.path);
        }, 0);
      }
    }
  }
}

let _probeBatchTimer = null;
let _scrollProbeTimer = null;
function probeVisibleAudio(immediate = false) {
  clearTimeout(_scrollProbeTimer);
  if (!immediate) {
    _scrollProbeTimer = setTimeout(() => probeVisibleAudio(true), 120);
    return;
  }
  const box = $('#files');
  const files = state.files;
  if (!box || !files || !files.length) return;
  const scroll = box.scrollTop;
  const viewH = box.clientHeight || 300;
  const total = files.length;
  const rowH = getRowH();
  let start = Math.max(0, Math.floor(scroll / rowH) - 2);
  let end = Math.min(total, Math.ceil((scroll + viewH) / rowH) + 2);
  if (total <= 40) { start = 0; end = total; }

  const slice = files.slice(start, end).filter((f) => f.isAudio && !isMidiFile(f) && (!state.probeCache[f.path] || !state.envCache?.[f.path]) && !state.probeInflight.has(f.path));
  slice.slice(0, 16).forEach((f) => {
    state.probeInflight.add(f.path);
    bridge('audio.probe', { path: f.path }).then((d) => {
      state.probeInflight.delete(f.path);
      if (d && d.ok) {
        if (d.duration) state.probeCache[f.path] = d.duration;
        if (d.envelope && d.envelope.length) {
          if (!state.envCache) state.envCache = {};
          state.envCache[f.path] = d.envelope;
          updateRowMiniWave(f.path);
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
  bridge('browser.search', { base: '', query: q, audioOnly: state.audioOnly, gen })
    .catch(() => {
      if (state.searchGen === gen) {
        state.searchPending = false;
        paintFromRaw(false);
      }
    });
}

function findSimilarSamples(f) {
  if (!f || !f.path) return;
  const rawName = f.name || f.filename || (f.path ? f.path.split(/[\\/]/).pop() : '') || '';
  state.similarSource = f.path;
  state.similarSourceName = rawName;
  state.searchQ = '';
  const searchInput = $('#search');
  if (searchInput) searchInput.value = '';
  paintLoadingFiles();
  bridge('browser.findSimilar', { path: f.path, limit: 30 }).then((data) => {
    const list = (data && data.results) ? data.results : [];
    for (const item of list) {
      const itemRaw = item.name || item.filename || (item.path ? item.path.split(/[\\/]/).pop() : '') || '';
      item.name = itemRaw;
      item.filename = itemRaw;
      if (item.isDir === undefined) item.isDir = false;
      if (item.isAudio === undefined) item.isAudio = !isMidiFile(item.path || itemRaw);
    }
    state.rawFiles = list;
    paintFromRaw(false);
    probeVisibleAudio();
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
  const list = (state.files && state.files.length) ? state.files : (state.rawFiles || []);
  if (!list || !list.length) return;
  let i = list.findIndex((f) => f.path === state.selected);
  if (i < 0) {
    i = delta > 0 ? 0 : list.length - 1;
  } else {
    i = Math.max(0, Math.min(list.length - 1, i + delta));
  }
  const f = list[i];
  if (!f) return;
  selectEntry(f);

  const box = $('#files');
  if (box && box.offsetParent !== null) {
    const rowH = getRowH();
    const itemTop = i * rowH;
    const itemBottom = itemTop + rowH;
    const clientH = box.clientHeight || 300;

    if (itemBottom > box.scrollTop + clientH) {
      box.scrollTop = itemBottom - clientH + 10;
    } else if (itemTop < box.scrollTop) {
      box.scrollTop = Math.max(0, itemTop - 10);
    }
    paintVisible();
  } else {
    const tree = $('#tree');
    const curTreeSel = tree?.querySelector(`.tree-row.side-file[title="${CSS.escape(f.path)}"]`);
    if (curTreeSel) {
      curTreeSel.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
    }
  }
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

  const searchClear = $('#searchClear');
  if (searchClear && searchInput) {
    searchClear.onclick = () => {
      searchInput.value = '';
      searchClear.classList.add('hidden');
      state.searchQ = '';
      if (searchPop) searchPop.classList.add('hidden');
      state.searchPending = false;
      state.searchGen = ++state.searchSeq;
      if (state.favOnly) {
        bridge('browser.getFavoriteEntries').then((res) => {
          const files = Array.isArray(res) ? res : (res && res.files ? res.files : []);
          state.rawFiles = files || [];
          paintFromRaw(false);
        }).catch(() => {
          state.rawFiles = [];
          paintFromRaw(false);
        });
      } else if (state.currentDir) {
        loadDir(state.currentDir, false);
      } else {
        state.rawFiles = [];
        paintFromRaw(false);
      }
      searchInput.focus();
    };
  }

  if (searchInput) {
    searchInput.addEventListener('input', (e) => {
      clearTimeout(searchTimer);
      const val = e.target.value;
      if (searchClear) {
        searchClear.classList.toggle('hidden', !val);
      }
      updateSuggestions(val);
      searchTimer = setTimeout(() => {
        state.searchQ = val;
        const q = (state.searchQ || '').trim();
        if (q) runSearch(q);
        else {
          state.searchPending = false;
          state.searchGen = ++state.searchSeq;
          if (state.favOnly) {
            bridge('browser.getFavoriteEntries').then((res) => {
              const files = Array.isArray(res) ? res : (res && res.files ? res.files : []);
              state.rawFiles = files || [];
              paintFromRaw(false);
            }).catch(() => {
              state.rawFiles = [];
              paintFromRaw(false);
            });
          } else if (state.currentDir) {
            loadDir(state.currentDir, false);
          } else {
            state.rawFiles = [];
            paintFromRaw(false);
          }
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
    if (state.currentDir && !(state.searchQ || '').trim() && !state.favOnly) loadDir(state.currentDir, true);
    else paintFromRaw(true);
  };
  const favBtn = $('#favOnly');
  if (favBtn) {
    favBtn.title = tr('browser.favOnly');
    favBtn.onclick = async (e) => {
      state.favOnly = !state.favOnly;
      e.target.classList.toggle('on', state.favOnly);
      if (state.favOnly) {
        state.savedDirBeforeFav = state.currentDir;
        const box = $('#files');
        if (box && state.currentDir) {
          state.dirScrolls[state.currentDir] = box.scrollTop;
        }
        try {
          const res = await bridge('browser.getFavoriteEntries');
          const files = Array.isArray(res) ? res : (res && res.files ? res.files : []);
          state.rawFiles = files || [];
          state.favSet = new Set((state.rawFiles || []).map((f) => f.path));
        } catch {
          state.rawFiles = [];
        }
        paintFromRaw(false);
      } else {
        if (state.savedDirBeforeFav) {
          state.currentDir = state.savedDirBeforeFav;
          loadDir(state.currentDir, false);
        } else {
          paintFromRaw(false);
        }
      }
    };
  }
  const tagSel = $('#tagFilter');
  if (tagSel) tagSel.onchange = (e) => { state.tagFilter = +e.target.value; paintFromRaw(true); };
  $('#btnRefresh').onclick = () => {
    state.subCache = {};
    if (state.favOnly) {
      bridge('browser.getFavoriteEntries').then((res) => {
        const files = Array.isArray(res) ? res : (res && res.files ? res.files : []);
        state.rawFiles = files || [];
        paintFromRaw(false);
      });
    } else if (state.currentDir) loadDir(state.currentDir, true);
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
  const filesBox = $('#files');
  filesBox.addEventListener('scroll', () => {
    if (state.currentDir && !(state.searchQ || '').trim() && !state.favOnly) {
      state.dirScrolls[state.currentDir] = filesBox.scrollTop;
    }
    paintVisible();
    clearTimeout(_scrollProbeTimer);
    _scrollProbeTimer = setTimeout(probeVisibleAudio, 100);
  });

  filesBox.onclick = (e) => {
    if (state._suppressClick) return;
    if (e.target.closest('#similarBanner') || e.target.closest('.similar-banner')) return;
    const row = e.target.closest('.file-row');
    if (row && row._path) {
      const f = (state.files || []).find((x) => x.path === row._path);
      if (f) selectEntry(f);
      return;
    }
    const rect = filesBox.getBoundingClientRect();
    const clickY = (e.clientY - rect.top) + filesBox.scrollTop;
    const rowH = getRowH();
    const banner = $('#similarBanner');
    const bannerH = (banner && !banner.classList.contains('hidden')) ? banner.offsetHeight : 0;
    const idx = Math.floor((clickY - bannerH) / rowH);
    if (state.files && idx >= 0 && idx < state.files.length) {
      selectEntry(state.files[idx]);
    }
  };

  $('#btnPlay').onclick = (e) => {
    if (e && e.currentTarget && typeof e.currentTarget.blur === 'function') {
      e.currentTarget.blur();
    }
    if (state.playing) {
      stopMidiPlayback();
      bridge('audio.stop').then(refreshPlayState);
    } else if (state.selected) {
      playFile(state.selected);
    }
  };
  $('#btnLoop').onclick = () => {
    state.loop = !state.loop;
    $('#btnLoop')?.classList.toggle('on', state.loop);
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
      const pop = $('#pianoTransposerPop');
      if (pop) {
        pop.classList.toggle('hidden');
        if (!pop.classList.contains('hidden')) {
          updateTransposerPopUI();
        }
      }
    };
  }
  $$('#pianoKeyboard .piano-key').forEach((k) => {
    k.onclick = (e) => {
      e.stopPropagation();
      const targetNote = k.dataset.note;
      setTargetNote(targetNote);
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

  const waveCanvas = $('#waveform');
  if (waveCanvas) {
    waveCanvas.title = tr('player.dragTip') || 'Kéo vào REAPER';
    armOleDrag(waveCanvas, () => {
      const curPath = state.selected || state.playingPath;
      if (!curPath) return null;
      const f = (state.files || []).find((x) => x.path === curPath);
      return f || { path: curPath, isAudio: !isMidiFile(curPath) };
    });
    waveCanvas.addEventListener('click', (e) => {
      if (state._suppressClick) return;
      if (!state.duration) return;
      const r = waveCanvas.getBoundingClientRect();
      const frac = Math.max(0, Math.min(1, (e.clientX - r.left) / r.width));
      bridge('audio.seek', { fraction: frac }).catch(() => {});
      state.position = frac;
      drawWaveform();
    });
  }

  const progTrack = $('#meterSeekWrap') || $('#kawaiiProgressTrack');
  if (progTrack) {
    progTrack.addEventListener('pointerdown', (e) => {
      if (!state.duration) return;
      const r = progTrack.getBoundingClientRect();
      const frac = Math.max(0, Math.min(1, (e.clientX - r.left) / r.width));
      bridge('audio.seek', { fraction: frac }).catch(() => {});
      state.position = frac;
      drawWaveform();
      drawMeter();
    });
  }

  const trackInfoEl = $('#trackInfo');
  if (trackInfoEl) {
    trackInfoEl.style.cursor = 'grab';
    trackInfoEl.title = tr('player.dragTip') || 'Kéo vào REAPER';
    armOleDrag(trackInfoEl, () => {
      const curPath = state.selected || state.playingPath;
      if (!curPath) return null;
      const f = (state.files || []).find((x) => x.path === curPath);
      return f || { path: curPath, isAudio: !isMidiFile(curPath) };
    });
  }

  document.addEventListener('click', (e) => {
    if (!e.target.closest('#ctxMenu')) $('#ctxMenu')?.classList.add('hidden');
    if (!e.target.closest('#pianoTransposerPop') && !e.target.closest('#btnKeyTransposer')) $('#pianoTransposerPop')?.classList.add('hidden');
  });
  let _resizeDebounce = null;
  window.addEventListener('resize', () => {
    drawWaveform();
    paintVisible();
    const isNarrow = isNarrowTreeMode();
    if (isNarrow !== state._lastNarrowMode) {
      state._lastNarrowMode = isNarrow;
      clearTimeout(_resizeDebounce);
      _resizeDebounce = setTimeout(() => {
        renderTree();
      }, 50);
    }
  });
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
  // 1. Lock DevTools (F12, Ctrl+Shift+I, Ctrl+Shift+J, Ctrl+Shift+C)
  if (e.key === 'F12' ||
      ((e.ctrlKey || e.metaKey) && e.shiftKey && (e.key === 'I' || e.key === 'i' || e.key === 'J' || e.key === 'j' || e.key === 'C' || e.key === 'c'))) {
    e.preventDefault();
    e.stopPropagation();
    return;
  }

  // 2. Lock View Source (Ctrl+U), Print (Ctrl+P), Save (Ctrl+S), Caret browsing (F7), Find Next (F3)
  if (((e.ctrlKey || e.metaKey) && (e.key === 'u' || e.key === 'U' || e.key === 'p' || e.key === 'P' || e.key === 's' || e.key === 'S')) ||
      e.key === 'F3' || e.key === 'F7') {
    e.preventDefault();
    e.stopPropagation();
    return;
  }

  // 3. Lock Browser Zoom (Ctrl + +, Ctrl + -, Ctrl + 0, Ctrl + =)
  if ((e.ctrlKey || e.metaKey) && (e.key === '+' || e.key === '=' || e.key === '-' || e.key === '_' || e.key === '0')) {
    e.preventDefault();
    e.stopPropagation();
    return;
  }

  // 4. Lock / Override Ctrl+F & Ctrl+K to focus Reals Lab Search
  if ((e.ctrlKey || e.metaKey) && (e.key === 'f' || e.key === 'F' || e.key === 'k' || e.key === 'K')) {
    e.preventDefault();
    e.stopPropagation();
    if (state.tab !== 'browser') {
      state.tab = 'browser';
      renderNav();
      showTab('browser');
    }
    const s = $('#search');
    if (s) { s.focus(); s.select(); }
    return;
  }

  // 5. Support F5 / Ctrl+R to reload UI (both in browser and in WebView2)
  if (e.key === 'F5' || ((e.ctrlKey || e.metaKey) && (e.key === 'r' || e.key === 'R'))) {
    e.preventDefault();
    e.stopPropagation();
    window.location.reload();
    return;
  }
  if (typingInField()) {
    if (e.key === 'Escape') { e.target.blur(); e.preventDefault(); }
    else if (e.key === 'ArrowDown' || e.key === 'Down') {
      e.target.blur();
      e.preventDefault();
      moveSelection(1);
    }
    return;
  }
  // ---------------------------------------------------------------------------
  // Spacebar Behavior Rule (DAW Producer / Beatmaker Workflow):
  // When typing in a text/search input field: allow normal space character entry.
  // When browsing files / navigating the UI: Spacebar MUST toggle the DAW (REAPER)
  // project transport (Play/Stop), allowing producers to start/stop their arrangement
  // without losing focus or switching windows. Sample preview in Reals Lab is triggered
  // by selection / Enter / Play button.
  // (QUY TẮC: Phím cách khi duyệt file BẮT BUỘC dùng để Play/Stop bài nhạc trong DAW).
  // ---------------------------------------------------------------------------
  if (e.key === ' ' || e.code === 'Space') {
    if (typingInField()) {
      return; // allow typing space in search boxes
    }
    e.preventDefault();
    e.stopPropagation();
    if (state.playing) {
      stopMidiPlayback();
      state.playing = false;
      refreshPlayState(true);
    }
    bridge('reaper.playToggle').catch(() => {});
    return;
  }
  if (state.tab !== 'browser') return;
  if (e.key === 'ArrowDown' || e.key === 'Down') { e.preventDefault(); moveSelection(1); }
  else if (e.key === 'ArrowUp' || e.key === 'Up') { e.preventDefault(); moveSelection(-1); }
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

let _playFileSeq = 0;

// ============ Preview / audio ============
// Stop any currently-playing file before starting a new one — without this,
// auto-preview clicking fast through the file list can trigger overlapping
// decoders in the C++ audio backend.
async function playFile(path) {
  if (!path) return;
  const mySeq = ++_playFileSeq;
  try {
    if (state.playing) {
      try { await bridge('audio.stop'); } catch {}
      state.playing = false;
    }
    if (mySeq !== _playFileSeq) return;

    if (isMidiFile(path)) {
      state.playingPath = path;
      state.selected = path;
      state.envelope = [];
      const filename = path.split(/[\\/]/).pop() || '';
      const info = $('#trackInfo');
      if (info) info.textContent = `${filename} | MIDI Sequence`;
      const tags = extractTagsFromFilename(filename);
      state.sampleTags = tags;
      renderPlayerTags(tags);

      const parsed = await loadMidiPreviewData(path);
      if (mySeq !== _playFileSeq) return;
      if (parsed && parsed.notes && parsed.notes.length > 0) {
        state.midiNotes = parsed.notes;
        state.duration = parsed.duration || 4.0;
        state.originalRootNote = parsed.tonic || 'C';
        if (state.isUserTargetKeyLocked && state.userTargetNote) {
          state.selectedTargetNote = state.userTargetNote;
          state.pitchSemitones = calculateSemitoneDistance(parsed.tonic, state.userTargetNote);
        } else {
          state.selectedTargetNote = parsed.tonic;
          state.pitchSemitones = 0;
        }
        updateTransposerPopUI();

        state.playing = true;
        const bp = $('#btnPlay');
        if (bp) {
          bp.textContent = '❚❚';
          bp.classList.add('playing');
        }
        startPlayerAnimLoop();
        playMidiEvents(parsed.notes, parsed.duration);
        drawWaveform();
      }
      return;
    }
    const fileObj = (state.files || []).find((x) => x.path === path);
    const filename = path.split(/[\\/]/).pop() || '';
    const bpmMatch = filename.match(/(\d+)\s*bpm/i);
    const filenameBpm = bpmMatch ? parseFloat(bpmMatch[1]) : 0;
    const filenameKey = extractKeyFromFilename(filename) || '';

    const initialBpm = (fileObj && fileObj.bpm > 0) ? fileObj.bpm : filenameBpm;
    const initialKey = (fileObj && fileObj.key) ? fileObj.key : (filenameKey || 'C');

    state.sampleBpm = initialBpm || 0;
    state.sampleKey = initialKey;

    const rootNote = extractRootNoteName(initialKey);
    state.originalRootNote = rootNote;
    let initialPitchShift = 0;
    if (state.isUserTargetKeyLocked && state.userTargetNote) {
      state.selectedTargetNote = state.userTargetNote;
      initialPitchShift = calculateSemitoneDistance(rootNote, state.userTargetNote);
    } else {
      state.selectedTargetNote = rootNote;
      initialPitchShift = state.pitchSemitones || 0;
    }
    state.pitchSemitones = initialPitchShift;
    updateTransposerPopUI();

    const sampleBpm = (fileObj && fileObj.bpm) || (state.selected === path ? state.sampleBpm : 0) || 0;
    const isSyncActive = $('#btnSyncBpm')?.classList.contains('on') || !!state.syncBpm;
    state.syncBpm = isSyncActive;
    const d = await bridge('audio.play', {
      path,
      loop: state.loop,
      syncBpm: isSyncActive,
      sampleBpm: sampleBpm,
      pitchSemitones: initialPitchShift
    });
    if (mySeq !== _playFileSeq) return;
    if (!d || d.ok === false) {
      console.warn('audio.play failed for:', path, d);
      toast(tr('toast.decodeFail'));
      return;
    }
    state.playingPath = path;
    state.selected = path;
    state.envelope = d.envelope || [];
    state.duration = d.duration || 0;
    // Phase-synced entry starts mid-loop — jump the playhead straight to the
    // audible position instead of animating from 0.
    state.position = (d.phaseSynced && typeof d.startFraction === 'number')
      ? Math.min(0.999, Math.max(0, d.startFraction)) : 0;
    if (typeof d.timeRatio === 'number' && d.timeRatio > 0) {
      state.timeRatio = d.timeRatio;
    }
    state.playing = true;
    if (d.duration) state.probeCache[path] = d.duration;
    const bp = $('#btnPlay');
    if (bp) {
      bp.textContent = '❚❚';
      bp.classList.add('playing');
    }

    const info = $('#trackInfo');
    if (info) info.textContent = `♪ ${filename} | ${d.sampleRate || 44100}Hz ${d.channels || 2}ch`;

    // Dynamic player tags extraction
    try {
      const tags = extractTagsFromFilename(filename);
      state.sampleTags = tags;
      renderPlayerTags(tags);
    } catch {}

    // BPM & Key extraction: check hydrated file entry, then filename regex, then background DB/DSP meta
    try {
      // Try to get real metadata from DB / detection
      bridge('audio.getSampleMeta', { path }).then((meta) => {
        if (mySeq !== _playFileSeq) return;
        let bpm = 0, key = '';
        let genre = '', mood = '';
        if (meta && meta.ok !== false) {
          if (meta.bpm && meta.bpm > 30) bpm = meta.bpm;
          if (meta.key) key = meta.key;
          if (meta.genre) genre = meta.genre;
          if (meta.mood) mood = meta.mood;
        }
        if (!bpm) bpm = initialBpm;
        if (!key || key === 'ORIGINAL') key = initialKey;

        state.sampleBpm = bpm || 0;
        state.sampleKey = key || 'C';

        const detectedRoot = extractRootNoteName(state.sampleKey);
        state.originalRootNote = detectedRoot;
        if (state.isUserTargetKeyLocked && state.userTargetNote) {
          state.selectedTargetNote = state.userTargetNote;
          state.pitchSemitones = calculateSemitoneDistance(detectedRoot, state.userTargetNote);
        } else {
          state.selectedTargetNote = detectedRoot;
          state.pitchSemitones = 0;
        }

        updateTransposerPopUI();
        if (state.pitchSemitones !== 0) {
          bridge('audio.setPitchShift', { semitones: state.pitchSemitones }).catch(()=>{});
        }

        // Render tags from DB if available, else heuristic
        if (genre || mood) {
          const tags = [];
          if (genre) tags.push({ name: genre, type: 'genre' });
          if (mood) tags.push({ name: mood, type: 'mood' });
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
        if (mySeq !== _playFileSeq) return;
        state.sampleBpm = filenameBpm;
        state.sampleKey = filenameKey;
        updateTransposerPopUI();
      });
    } catch {}

    const keyLabel = $('#playerKeyLabel');
    if (keyLabel && !keyLabel.textContent) keyLabel.textContent = state.selectedTargetNote || 'C';

    startPlayerAnimLoop();
    drawWaveform();
  } catch (err) {
    if (mySeq === _playFileSeq) {
      console.error('playFile error:', err);
      toast(tr('toast.decodeFail'));
    }
  }
}

let _meterSmoothedVal = 0;
let _playerRafId = null;

function startPlayerAnimLoop() {
  if (_playerRafId) return;
  let lastTime = performance.now();

  function step(now) {
    if (!state.playing) {
      _meterSmoothedVal = 0;
      _playerRafId = null;
      drawMeterSmoothed(0);
      drawWaveform();
      return;
    }

    const dt = Math.min(0.08, (now - lastTime) / 1000);
    lastTime = now;

    // Smoothly extrapolate position at 60 FPS.
    // Position is a fraction of the OUTPUT timeline (what the listener hears),
    // so the advance rate is 1/outputDuration = timeRatio/rawDuration. Using
    // rawDuration here made the playhead lag behind DSP-stretched playback.
    if (state.duration > 0) {
      const outDuration = state.duration / (state.timeRatio || 1);
      if (state.loop) {
        state.position = (state.position + dt / outDuration) % 1.0;
      } else {
        const nextPos = state.position + dt / outDuration;
        if (nextPos >= 1.0) {
          state.position = 0;
          state.playing = false;
          _meterSmoothedVal = 0;
          const bp = $('#btnPlay');
          if (bp) {
            bp.textContent = '▶';
            bp.classList.remove('playing');
          }
          const timeLbl = $('#timeLabel');
          if (timeLbl) timeLbl.textContent = `0.0 / ${state.duration.toFixed(1)}s`;
          drawMeterSmoothed(0);
          drawWaveform();
          bridge('audio.stop').catch(() => {});
          _playerRafId = null;
          return;
        } else {
          state.position = nextPos;
        }
      }
    }

    // Target peak from envelope at current position
    const curPath = state.selected || state.playingPath || '';
    const env = (state.envelope && state.envelope.length) ? state.envelope : ((state.envCache && state.envCache[curPath]) || []);
    let targetPeak = state.peak || 0;
    if (env && env.length > 0) {
      const idx = Math.min(env.length - 1, Math.floor(Math.max(0, state.position) * env.length));
      targetPeak = Math.max(targetPeak, env[idx] || 0);
    } else {
      targetPeak = Math.max(targetPeak, 0.4);
    }

    // Analog hardware VU meter ballistics: Instant attack, smooth exponential decay
    if (targetPeak > _meterSmoothedVal) {
      _meterSmoothedVal = targetPeak; // Instant transient attack
    } else {
      _meterSmoothedVal = Math.max(0, _meterSmoothedVal - dt * 3.5); // Organic smooth decay
    }

    const timeLbl = $('#timeLabel');
    if (timeLbl) timeLbl.textContent = `${(state.position * state.duration).toFixed(1)} / ${state.duration.toFixed(1)}s`;
    drawMeterSmoothed(_meterSmoothedVal);
    drawWaveform();

    _playerRafId = requestAnimationFrame(step);
  }
  _playerRafId = requestAnimationFrame(step);
}

function refreshPlayState(keepPosition = false) {
  state.playing = false;
  if (!keepPosition) {
    state.position = 0;
  }
  state.timeRatio = 1;
  if (_playerRafId) {
    cancelAnimationFrame(_playerRafId);
    _playerRafId = null;
  }
  _meterSmoothedVal = 0;
  const bp = $('#btnPlay');
  if (bp) {
    bp.textContent = '▶';
    bp.classList.remove('playing');
  }
  const timeLbl = $('#timeLabel');
  if (timeLbl && state.duration > 0) {
    const curSec = (state.position * state.duration).toFixed(1);
    timeLbl.textContent = `${curSec} / ${state.duration.toFixed(1)}s`;
  }
  drawMeterSmoothed(0);
  drawWaveform();
}

function updatePreviewLive() {
  const timeLbl = $('#timeLabel');
  if (timeLbl) timeLbl.textContent = `${(state.position * state.duration).toFixed(1)} / ${state.duration.toFixed(1)}s`;
  if (!state.playing) {
    drawMeterSmoothed(0);
    drawWaveform();
  }
}

function resizeCanvasIfNeeded(c, hOverride) {
  if (!c) return { W: 0, H: 0 };
  const w = Math.round(c.clientWidth || 300);
  const h = hOverride || Math.round(c.clientHeight || 44);
  if (Math.abs(c.width - w) > 1) c.width = w;
  if (Math.abs(c.height - h) > 1) c.height = h;
  return { W: c.width, H: c.height };
}

function drawWaveform() {
  const c = $('#waveform');
  if (!c) return;
  const { W, H } = resizeCanvasIfNeeded(c, 44);
  if (W <= 0 || H <= 0) return;
  const ctx = c.getContext('2d');
  ctx.clearRect(0, 0, W, H);
  const curPath = state.selected || state.playingPath || '';

  if (isMidiFile(curPath)) {
    // Piano Roll Canvas Rendering for MIDI files
    const rows = 8;
    const cols = 16;
    const cellH = H / rows;
    const cellW = W / cols;

    // Background grid
    ctx.fillStyle = canvasThemeColors.pianorollBg;
    ctx.fillRect(0, 0, W, H);

    ctx.strokeStyle = canvasThemeColors.pianorollGrid;
    ctx.lineWidth = 1;
    for (let r = 0; r <= rows; ++r) {
      ctx.beginPath();
      ctx.moveTo(0, r * cellH);
      ctx.lineTo(W, r * cellH);
      ctx.stroke();
    }
    for (let col = 0; col <= cols; ++col) {
      ctx.beginPath();
      ctx.moveTo(col * cellW, 0);
      ctx.lineTo(col * cellW, H);
      ctx.stroke();
    }

    const notes = state.midiNotes;
    const dur = state.duration || 4.0;
    const shift = state.pitchSemitones || 0;

    if (notes && notes.length > 0) {
      let minNote = 127, maxNote = 0;
      notes.forEach(n => {
        minNote = Math.min(minNote, n.note);
        maxNote = Math.max(maxNote, n.note);
      });
      if (minNote > maxNote) { minNote = 48; maxNote = 72; }
      const noteRange = Math.max(12, maxNote - minNote + 2);
      const curTime = state.position * dur;

      notes.forEach((n) => {
        const nx = (n.time / dur) * W;
        const nw = Math.max(3, (n.duration / dur) * W - 1);
        const transposedMidi = n.note + shift;
        const normPitch = (transposedMidi - minNote) / noteRange;
        const ny = H - 6 - Math.max(0, Math.min(H - 8, normPitch * (H - 8)));
        const nh = Math.max(3, cellH - 1);

        const isNoteActive = state.playing && (curTime >= n.time && curTime <= n.time + n.duration);

        const grad = ctx.createLinearGradient(nx, ny, nx + nw, ny);
        if (isNoteActive) {
          grad.addColorStop(0, canvasThemeColors.pianorollNoteActive);
          grad.addColorStop(1, canvasThemeColors.pianorollNote);
        } else {
          grad.addColorStop(0, canvasThemeColors.pianorollNote);
          grad.addColorStop(1, canvasThemeColors.pianorollNoteGradEnd);
        }
        ctx.fillStyle = grad;
        ctx.beginPath();
        if (ctx.roundRect) ctx.roundRect(nx, ny, nw, nh, 1.5);
        else ctx.rect(nx, ny, nw, nh);
        ctx.fill();
      });
    } else {
      // Fallback
      let pathHash = 0;
      for (let i = 0; i < curPath.length; ++i) {
        pathHash = (Math.imul(31, pathHash) + curPath.charCodeAt(i)) | 0;
      }
      const baseRootIdx = NOTE_NAMES.indexOf(state.originalRootNote || 'C');
      const numNotes = 20;
      for (let i = 0; i < numNotes; ++i) {
        const step = ((i * 3 + ((pathHash >> (i % 8)) & 3)) % cols);
        const span = 1 + (((pathHash >> ((i + 2) % 8)) & 3) % 3);
        const noteDegree = ((i * 2 + (pathHash & 7)) % 12);
        const transposedPitch = (baseRootIdx + noteDegree + shift + 120) % 12;
        const pitchRow = 7 - Math.floor((transposedPitch / 12) * 8);

        const nx = step * cellW + 1;
        const ny = pitchRow * cellH + 1.5;
        const nw = Math.max(8, span * cellW - 2);
        const nh = Math.max(2, cellH - 3);

        const grad = ctx.createLinearGradient(nx, ny, nx + nw, ny);
        grad.addColorStop(0, canvasThemeColors.pianorollNote);
        grad.addColorStop(1, canvasThemeColors.pianorollNoteGradEnd);
        ctx.fillStyle = grad;
        ctx.beginPath();
        if (ctx.roundRect) ctx.roundRect(nx, ny, nw, nh, 2);
        else ctx.rect(nx, ny, nw, nh);
        ctx.fill();
      }
    }

    // Playhead line
    const px = (W - 2) * Math.min(1, Math.max(0, state.position));
    if (state.playing || state.position > 0) {
      ctx.fillStyle = canvasThemeColors.waveformPlayhead;
      ctx.fillRect(px, 0, 1.2, H);
    }
    return;
  }

  // Audio Waveform Rendering
  const env = (state.envelope && state.envelope.length) ? state.envelope : ((state.envCache && state.envCache[curPath]) || []);
  const mid = H / 2;
  const amp = H / 2 - 3;
  const px = (W - 2) * Math.min(1, Math.max(0, state.position));

  // Centerline
  ctx.fillStyle = canvasThemeColors.waveformCenterline;
  ctx.fillRect(0, mid - 0.5, W, 1);

  if (env && env.length) {
    const numBars = env.length;
    const barW = W / numBars;
    const drawW = Math.max(1, barW - 1.2); // Airy, separated micro-bars

    for (let i = 0; i < numBars; ++i) {
      const x = i * barW;
      const val = env[i];
      const curved = Math.pow(Math.min(1, val), 0.75);
      const h = Math.max(1, curved * amp);
      const isPlayed = (x + barW / 2) <= px;

      ctx.fillStyle = isPlayed ? canvasThemeColors.waveformFillActive : canvasThemeColors.waveformFill;

      const barY = mid - h;
      const barH = Math.max(2, h * 2);
      if (ctx.roundRect) {
        ctx.beginPath();
        ctx.roundRect(x, barY, drawW, barH, 0.6);
        ctx.fill();
      } else {
        ctx.fillRect(x, barY, drawW, barH);
      }
    }

    // Playhead cursor
    if (state.playing || state.position > 0) {
      ctx.fillStyle = canvasThemeColors.waveformPlayhead;
      ctx.fillRect(px, 0, 1.2, H);
    }
  } else {
    // Subtle idle placeholder
    ctx.fillStyle = canvasThemeColors.waveformCenterline;
    ctx.fillRect(0, mid - 0.5, W, 1);
  }

}

function drawMeterSmoothed(peak) {
  const c = $('#meter');
  if (!c) return;
  const { W, H } = resizeCanvasIfNeeded(c, 6);
  if (W <= 0 || H <= 0) return;
  const ctx = c.getContext('2d');
  ctx.clearRect(0, 0, W, H);

  const curTheme = window.themeManager ? window.themeManager.getTheme() : 'dark-studio';
  const isPastel = (curTheme === 'pastel-pink');

  // Background track for pastel theme
  if (isPastel) {
    ctx.fillStyle = '#FDF0E9';
    if (ctx.roundRect) {
      ctx.beginPath();
      ctx.roundRect(0, 0, W, H, H / 2);
      ctx.fill();
    } else {
      ctx.fillRect(0, 0, W, H);
    }
  }

  const fillW = (!state.playing || peak <= 0.005) ? 0 : Math.max(2, W * Math.min(1, Math.pow(peak, 0.75) * 1.05));

  if (fillW > 0) {
    if (isPastel) {
      ctx.fillStyle = '#FF8DA6';
      if (ctx.roundRect) {
        ctx.beginPath();
        ctx.roundRect(0, 0, fillW, H, H / 2);
        ctx.fill();
      } else {
        ctx.fillRect(0, 0, fillW, H);
      }
    } else {
      const grad = ctx.createLinearGradient(0, 0, W, 0);
      grad.addColorStop(0, canvasThemeColors.meterFill);
      grad.addColorStop(0.70, canvasThemeColors.meterFill);
      grad.addColorStop(0.85, canvasThemeColors.meterFillWarn);
      grad.addColorStop(1, canvasThemeColors.meterFillClip);
      ctx.fillStyle = grad;
      ctx.fillRect(0, 0, fillW, H);
    }
  }

  const heart = $('#kawaiiHeartScrubber');
  if (heart) {
    if (!state.playing || fillW <= 0) {
      heart.style.left = '0%';
    } else {
      const frac = fillW / (W || 1);
      heart.style.left = (frac * 100).toFixed(2) + '%';
    }
  }
}

function drawMeter() {
  drawMeterSmoothed(_meterSmoothedVal || state.peak || 0);
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

// Exit the search mode (clear query + input + state) so that the header
// and the freshly reloaded folder list reflect reality after a delete or
// rename. Without this, loadDir() repopulates rawFiles from the parent dir
// but searchQ stays set, leaving the header saying "Search results: N"
// while the list actually shows the whole folder.
function exitSearchMode() {
  if (!(state.searchQ || '').trim()) return; // nothing to clear
  state.searchQ = '';
  state.searchPending = false;
  state.searchGen = ++state.searchSeq;
  state.listDir = null;
  const s = $('#search');
  if (s) s.value = '';
  const sc = $('#searchClear');
  if (sc) sc.classList.add('hidden');
}

function startRename(f) {
  if (!f || f.isDir) return;
  $('#renameModal').classList.remove('hidden');
  // Pre-select just the name stem (without extension) so typing a new name
  // doesn't accidentally wipe the extension, and so Enter renames only the
  // stem. We also auto-append the original extension below if the user
  // submits without one.
  const dotIdx = (f.name || '').lastIndexOf('.');
  const hasExt = dotIdx > 0 && dotIdx < f.name.length - 1;
  const stem = hasExt ? f.name.slice(0, dotIdx) : f.name;
  const ext = hasExt ? f.name.slice(dotIdx) : '';
  const input = $('#renameInput');
  input.value = f.name;
  input.focus();
  // Defer selection a bit so the modal focus/hover settles — 0ms was racy and
  // the caret ended up at the end of the field instead of selecting the stem.
  setTimeout(() => {
    try { input.setSelectionRange(0, stem.length); } catch (e) {}
  }, 50);
  $('#renameOk').onclick = () => {
    let newName = ($('#renameInput').value || '').trim();
    if (!newName) { $('#renameModal').classList.add('hidden'); return; }
    // Preserve the original extension if the user typed a name without one
    // (e.g. "My Kick" → "My Kick.wav"). If they explicitly typed a different
    // extension, keep their choice as-is.
    if (ext && !/\.[a-zA-Z0-9]+$/.test(newName)) {
      newName = newName + ext;
    }
    if (newName === f.name) { $('#renameModal').classList.add('hidden'); return; }
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
        exitSearchMode();
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
        exitSearchMode();
        loadDir(dir, true);
        renderTree();
      })
      .catch(() => { $('#deleteModal').classList.add('hidden'); toast(tr('toast.deleteFail')); });
  };
  $('#deleteCancel').onclick = () => $('#deleteModal').classList.add('hidden');
}

// ============ Context menus ============
function positionContextMenu(e) {
  const m = $('#ctxMenu');
  if (!m) return;
  m.style.visibility = 'hidden';
  m.classList.remove('hidden');

  const mw = m.offsetWidth || 210;
  const mh = m.offsetHeight || 240;
  const winW = window.innerWidth;
  const winH = window.innerHeight;

  // Default to cursor position; only flip/shrink when truly needed so the
  // menu stays anchored to the click point instead of jumping to a screen edge.
  let x = (typeof e.clientX === 'number' && e.clientX > 0) ? e.clientX : (e.pageX | 0) || 8;
  let y = (typeof e.clientY === 'number' && e.clientY > 0) ? e.clientY : (e.pageY | 0) || 8;

  // Flip left only if the menu would overflow the right edge.
  if (x + mw > winW - 8) {
    x = Math.max(8, winW - mw - 8);
  }
  // Flip up only if the menu would overflow the bottom edge — keep the top
  // anchored to the cursor when there is room (don't yank it to the top).
  if (y + mh > winH - 8) {
    y = Math.max(8, winH - mh - 8);
  }

  m.style.left = `${x}px`;
  m.style.top = `${y}px`;
  m.style.visibility = 'visible';
}

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
  positionContextMenu(e);
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
      .then((nowFav) => {
        if (nowFav) {
          state.favSet.add(f.path);
        } else {
          state.favSet.delete(f.path);
          if (state.favOnly) {
            state.rawFiles = (state.rawFiles || []).filter((item) => item.path !== f.path);
          }
        }
        renderTree();
        paintFromRaw(true);
      }),
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
  positionContextMenu(e);
}

function folderMenu(e, path) {
  const nPath = normPath(path);
  const isRoot = state.roots.some((r) => isSamePath(r.path, nPath));
  const name = nPath.split('\\').filter(Boolean).pop() || nPath;
  const items = [
    { label: tr('browser.ctx.scanNew'), action: () => triggerFolderScan(nPath, false) },
    { label: tr('browser.ctx.rescanAll'), action: () => triggerFolderScan(nPath, true) },
    { label: tr('browser.ctx.openHere'), action: () => { state.expanded.add(nPath); openDir(nPath); renderTree(); } },
    { label: tr('browser.ctx.setRoot'), action: () => bridge('fs.addRoot', { name, path: nPath }).then(() => refreshRoots()).then(() => toast(tr('toast.rootAdded'))) },
    { label: tr('browser.ctx.copyPath'), action: () => { navigator.clipboard?.writeText(nPath); toast(tr('toast.copied')); } },
    { label: tr('browser.ctx.reveal'), action: () => bridge('reaper.reveal', { path: nPath }) },
  ];
  if (isRoot) {
    items.push('-');
    items.push({ label: tr('browser.ctx.removeRoot'), action: () => bridge('fs.removeRoot', { path: nPath }).then(() => refreshRoots()).then(renderTree) });
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
    { n: 'JSFX Mastering Suite', ini: 'MS', c: '#3e4c6b', tag: { cls: 'pro', text: 'PRO' }, metaText: 'JSFX · 199K · Bao phuc Nguyen', btnText: tr('market.buy') },
    { n: 'Yumyoo Beatmaker Toolkit', ini: 'YT', c: '#524058', tag: { cls: 'pro', text: 'PRO' }, metaText: 'ReaScript Lua · 249K · Bao phuc Nguyen', btnText: tr('market.buy') },
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
      if (e.button === 0 && !state.docked) bridge('window.startDrag');
    });
  }

  $$('.resize-handle').forEach((h) => {
    h.addEventListener('mousedown', (e) => {
      if (e.button === 0 && !state.docked) {
        e.preventDefault();
        e.stopPropagation();
        const edge = h.className.replace('resize-handle', '').trim();
        bridge('window.startResize', { edge });
      }
    });
  });

  const btnAddFolder = $('#btnAddFolder');
  const folderInput = $('#folderInput');
  if (btnAddFolder && folderInput) {
    btnAddFolder.onclick = () => {
      folderInput.click();
    };
    folderInput.onchange = async (e) => {
      const files = e.target.files;
      if (!files || !files.length) return;
      const file = files[0];
      const fullPath = file.path || '';
      if (!fullPath) return;
      const sep = fullPath.includes('\\') ? '\\' : '/';
      const parts = fullPath.split(sep).filter(Boolean);
      // In webkitdirectory, file.path is the file inside the picked dir; get the root dir
      const dirPath = file.webkitRelativePath ? fullPath.slice(0, fullPath.indexOf(sep + file.webkitRelativePath.split('/')[0]) + (sep + file.webkitRelativePath.split('/')[0]).length) : fullPath;
      const name = parts[parts.length - 1] || 'Sample Library';
      try {
        await bridge('fs.addRoot', { name, path: dirPath || fullPath });
        await refreshRoots();
        openDir(dirPath || fullPath);
        renderTree();
        toast(tr('toast.rootAdded') || 'Đã thêm thư mục sample!');
      } catch (err) {
        console.warn('addRoot error:', err);
      }
      folderInput.value = '';
    };
  }

  const btnToggleTree = $('#btnToggleTree');
  if (btnToggleTree) {
    const tree = $('#tree');
    const isCol = tree ? tree.classList.contains('collapsed') : false;
    btnToggleTree.classList.toggle('on', !isCol);
    btnToggleTree.onclick = () => {
      if (tree) {
        tree.classList.toggle('collapsed');
        const collapsed = tree.classList.contains('collapsed');
        btnToggleTree.classList.toggle('on', !collapsed);
        localStorage.setItem('reals_tree_collapsed', collapsed ? 'true' : 'false');
        drawWaveform();
      }
    };
  }

  initLayoutSplitters();

  // Capture keydown at window level before any focused child element receives it
  window.addEventListener('keydown', onBrowserKey, { capture: true });

  // Capture keyup to prevent focused buttons from synthesizing click events on Space release
  window.addEventListener('keyup', (e) => {
    if (e.key === ' ' || e.code === 'Space') {
      if (!typingInField()) {
        e.preventDefault();
        e.stopPropagation();
      }
    }
  }, { capture: true });

  // Prevent buttons from retaining focus after click/pointerdown so Spacebar
  // never triggers a synthetic button click.
  document.addEventListener('pointerdown', (e) => {
    const btn = e.target.closest('button, [role="button"]');
    if (btn) {
      setTimeout(() => {
        if (document.activeElement === btn) btn.blur();
      }, 0);
    }
  });

  // Lock mouse wheel zoom (Ctrl + MouseWheel) and reset zoom to default
  window.addEventListener('wheel', (e) => {
    if (e.ctrlKey || e.metaKey) {
      e.preventDefault();
    }
  }, { passive: false });

  // Disable default browser context menu globally (retaining text selection for inputs)
  window.addEventListener('contextmenu', (e) => {
    const targetTag = (e.target && e.target.tagName ? e.target.tagName.toLowerCase() : '');
    if (targetTag !== 'input' && targetTag !== 'textarea') {
      if (!e.defaultPrevented) {
        e.preventDefault();
      }
    }
  });

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
        localStorage.setItem('reals_tree_width', Math.round(finalW));
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
        localStorage.setItem('reals_preview_height', Math.round(finalH));
        bridge('config.set', { key: 'previewHeight', value: Math.round(finalH) });
      };

      window.addEventListener('mousemove', onMove);
      window.addEventListener('mouseup', onUp);
    });
  }
}

boot();
