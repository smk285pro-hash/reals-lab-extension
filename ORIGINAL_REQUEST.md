# Original User Request

## 2026-08-31T18:18:25Z

<USER_REQUEST>
Implement Global Favorites (`★`), Global Search across all added folders, clean default root directories, and achieve zero-lag ultra-high performance file browsing in Reals Lab REAPER Extension.

Working directory: c:\Users\smk28\Desktop\reals lab extension
Integrity mode: development

## Requirements

### R1. Global Favorites View (`★`)
- When the Favorites toggle (`★` / `#favOnly`) is active, the file list must immediately display ALL favorited audio samples and MIDI files across the entire library (all roots and subfolders), regardless of the currently open folder.
- Favorited files can be previewed, transposed, dragged into REAPER, tagged, and un-favorited with live UI updates.

### R2. Global Search Across All Root Folders
- The search bar (`#search`) must search across ALL added root directories recursively, returning matching audio and MIDI files with instant (<50ms) response times.
- Search queries support text matching, `/tag`, `/bpm:range`, and `/key:note` filters.
- Clearing the search immediately restores the previous browsing view and scroll position.

### R3. Clean Initial Default Roots
- Remove hardcoded default directories (e.g. `Music`, `Desktop`, `Downloads`) from default initialization.
- Fresh extension instances start with a clean state prompting the user to add their sample library folders via the `+📁` button or drag-and-drop.

### R4. File Browsing Performance Optimization & Zero-Lag Benchmarking
- Profile and optimize directory listing (`fs.list`), search (`browser.search`), and tree expansion for directories with 5,000+ files.
- Ensure 60 FPS scrolling and zero UI hitching (<16ms frame time) via optimized virtual list rendering and debounced audio envelope probing.
- Verify zero memory leaks and thread safety across REAPER main thread, C++ worker threads, and WebView2 IPC bridge.

## Acceptance Criteria

### Automated & Objective Verification
- [ ] `cmake --build --preset windows` compiles with zero warnings and zero errors.
- [ ] `ctest --preset windows` passes 100% of test suites.
- [ ] Directory listing and global search for 5,000+ sample files complete in <30ms.
- [ ] Clicking `★` lists all favorited files across different directories in a single unified list.
- [ ] Searching a sample name present in any added root folder instantly returns the file in search results.
- [ ] Fresh installs contain 0 default OS folders (`Music`/`Desktop`/`Downloads`).
</USER_REQUEST>
