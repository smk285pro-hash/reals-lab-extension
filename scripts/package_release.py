import os
import shutil
import zipfile
from pathlib import Path

def main():
    root = Path(__file__).resolve().parent.parent
    dist_dir = root / "dist"
    pkg_name = "RealsLab_v1.0.0_Windows_Release"
    pkg_dir = dist_dir / pkg_name
    
    if pkg_dir.exists():
        shutil.rmtree(pkg_dir)
    pkg_dir.mkdir(parents=True, exist_ok=True)
    
    # 1. Copy Release DLL (Contains embedded ui-web assets)
    src_dll = root / "build" / "windows" / "extension" / "Release" / "reaper_realslab.dll"
    if not src_dll.exists():
        raise FileNotFoundError(f"DLL not found: {src_dll}")
    shutil.copy2(src_dll, pkg_dir / "reaper_realslab.dll")
    
    # 3. Create install.bat
    install_bat = pkg_dir / "install.bat"
    install_bat.write_text(
        "@echo off\n"
        "chcp 65001 >nul\n"
        "echo ========================================================\n"
        "echo       REALS LAB EXTENSION - INSTALLER CHO REAPER\n"
        "echo ========================================================\n"
        "set TARGET_DIR=%APPDATA%\\REAPER\\UserPlugins\n"
        "if not exist \"%TARGET_DIR%\" mkdir \"%TARGET_DIR%\"\n"
        "echo Đang sao chép reaper_realslab.dll vào %TARGET_DIR%...\n"
        "copy /Y \"%~dp0reaper_realslab.dll\" \"%TARGET_DIR%\\reaper_realslab.dll\" >nul\n"
        "if %ERRORLEVEL% equ 0 (\n"
        "    echo [THÀNH CÔNG] Đã cài đặt Reals Lab vào REAPER!\n"
        "    echo Bạn hãy mở REAPER, vào Actions -^> Show action list -^> tìm 'Reals Lab: Show Window'\n"
        ") else (\n"
        "    echo [LỖI] Không thể copy file. Hãy chắc chắn rằng REAPER đã được đóng trước khi cài đặt.\n"
        ")\n"
        "pause\n",
        encoding="utf-8"
    )
    
    # 4. Create README.txt
    readme = pkg_dir / "README.txt"
    readme.write_text(
        "========================================================\n"
        "        REALS LAB EXTENSION - PHIÊN BẢN RELEASE\n"
        "========================================================\n\n"
        "CÁCH CÀI ĐẶT:\n"
        "--------------------------------------------------------\n"
        "Cách 1 (Tự động - Khuyên dùng):\n"
        "  1. Đóng REAPER nếu đang mở.\n"
        "  2. Click đúp chuột vào file 'install.bat'.\n"
        "  3. Mở REAPER, nhấn '?' (hoặc vào menu Actions -> Show action list).\n"
        "  4. Tìm 'Reals Lab: Show Window' và gán phím tắt (hoặc Run).\n\n"
        "Cách 2 (Thủ công):\n"
        "  1. Copy file 'reaper_realslab.dll' vào thư mục:\n"
        "     C:\\Users\\<Tên_User>\\AppData\\Roaming\\REAPER\\UserPlugins\\\n"
        "  2. Khởi động REAPER và mở Action 'Reals Lab: Show Window'.\n\n"
        "LƯU Ý:\n"
        "  - Toàn bộ giao diện Web, biểu tượng và phông chữ ĐÃ ĐƯỢC NHÚNG TRỰC TIẾP 100% VÀO BÊN TRONG DLL.\n"
        "  - Người dùng chỉ cần duy nhất 1 file 'reaper_realslab.dll' là extension tự chạy hoàn hảo.\n"
        "========================================================\n",
        encoding="utf-8"
    )
    
    # 5. Create ZIP archive
    zip_path = dist_dir / f"{pkg_name}.zip"
    if zip_path.exists():
        zip_path.unlink()
        
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for file in pkg_dir.rglob("*"):
            if file.is_file():
                zf.write(file, file.relative_to(dist_dir))
                
    print(f"Package created successfully: {zip_path} ({zip_path.stat().st_size} bytes)")

if __name__ == '__main__':
    main()
