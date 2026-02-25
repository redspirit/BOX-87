#include "../IShellCommand.h"
#include "../shell.h"
#include "sdcard.h"
#include "palette.h"

#define SDSPEED_BUFFER  (32 * 1024 * 4)  // 128 KB

class CmdSdSpeed : public IShellCommand {
private:

    static const size_t BUFFER_SIZE = SDSPEED_BUFFER;

    uint8_t* _buffer = nullptr;

    char _path[MAX_PATH];

    uint64_t _fileSize = 0;
    uint64_t _totalRead = 0;

    uint32_t _startTime = 0;
    uint32_t _lastReport = 0;
    uint64_t _lastReadBytes = 0;

    bool _finished = false;
    bool _error = false;

public:

    void start(Shell& shell) override {
        auto& con = shell.console();
        auto& cmd = shell.parsedCmd();

        _finished = false;
        _error = false;

        if (cmd.argc() < 2) {
            con.setColor(COLOR_RED);
            con.printLn("Usage: SDSPEED <file>");
            con.useDefaultColor();
            _finished = true;
            return;
        }

        shell.resolvePath(cmd.argv(1), _path);

        if (!SDCARD::init()) {
            shell.console().setColor(COLOR_RED);
            shell.console().printLn("SD card not initialized");
            shell.console().useDefaultColor();
            _finished = true;
            return;
        }

        _fileSize = SDCARD::fileSize(_path);
        if (_fileSize == 0) {
            con.setColor(COLOR_RED);
            con.printLn("File not found or empty");
            con.useDefaultColor();
            _finished = true;
            return;
        }

        if (!SDCARD::open(_path)) {
            con.setColor(COLOR_RED);
            con.printLn("Failed to open file");
            con.useDefaultColor();
            _finished = true;
            return;
        }

        _buffer = (uint8_t*)malloc(BUFFER_SIZE);
        if (!_buffer) {
            con.setColor(COLOR_RED);
            con.printLn("Out of memory!");
            con.useDefaultColor();
            SDCARD::close();
            _finished = true;
            return;
        }

        _totalRead = 0;
        _startTime = millis();
        _lastReport = _startTime;
        _lastReadBytes = 0;

        con.printLn("Starting SD speed test...");
        con.printLn();
    }

    void tick(Shell& shell) override {

        if (_finished || _error)
            return;

        if (!SDCARD::available()) {
            finish(shell);
            return;
        }

        size_t toRead = BUFFER_SIZE;

        if (_fileSize - _totalRead < BUFFER_SIZE)
            toRead = _fileSize - _totalRead;

        size_t read = SDCARD::read(_buffer, toRead);

        if (read == 0) {
            finish(shell);
            return;
        }

        _totalRead += read;

        uint32_t now = millis();

        if (now - _lastReport >= 1000) {

            uint32_t interval = now - _lastReport;
            uint64_t intervalBytes = _totalRead - _lastReadBytes;

            if (interval == 0)
                interval = 1;

            uint32_t speedKB = (intervalBytes / 1024ULL) * 1000ULL / interval;

            uint32_t percent = (_totalRead * 100ULL) / _fileSize;

            auto& con = shell.console();

            con.printInt(percent); con.print("%  ");
            con.printInt((int)_totalRead); con.print(" bytes ");
            con.printInt(speedKB); con.printLn(" KB/s");

            _lastReport = now;
            _lastReadBytes = _totalRead;
        }
    }

    void cancel(Shell& shell) override {

        auto& con = shell.console();

        con.printLn();
        con.printLn("^C - Test cancelled");

        cleanup();

        _finished = true;
    }

    bool isFinished() const override {
        return _finished;
    }

    void onChar(Shell& shell, uint16_t c) override {
        // можно добавить горячие клавиши
        // например 'q' для выхода
        if (c == 'q' || c == 'Q') {
            cancel(shell);
        }
    }

private:

    void finish(Shell& shell) {

        uint32_t endTime = millis();
        uint32_t totalTime = endTime - _startTime;

        if (totalTime == 0)
            totalTime = 1;

        uint32_t avgKB =
            (_totalRead / 1024ULL) * 1000ULL / totalTime;

        auto& con = shell.console();

        con.printLn();
        con.printLn("=== SD SPEED SUMMARY ===");
        con.print("File size: "); con.printInt((int)_fileSize); con.printLn();
        con.print("Time: "); con.printInt((int)totalTime); con.printLn(" ms");
        con.print("Average speed: "); con.printInt(avgKB); con.printLn(" KB/s");

        cleanup();

        _finished = true;
    }

    void cleanup() {

        SDCARD::close();

        if (_buffer) {
            free(_buffer);
            _buffer = nullptr;
        }
    }
};