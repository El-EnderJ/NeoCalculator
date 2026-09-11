// Compiles actual startup/load bodies against a fake VFS; see the runner's
// hashes. The fake controls only I/O, never mount flags or startup decisions.
#include "FS.h"
#include "math/VariableManager.h"
#include "demo/DemoSettingsRecord.h"
#include "pinned_begin.inc"

namespace vpam {
ExactVal ExactVal::fromInt(int64_t n) { ExactVal v; v.num = n; return v; }
enum class AngleMode { DEG, RAD };
}
namespace numos {
inline bool testDeg = true;
bool angleModeIsDeg() { return testDeg; }
void setAngleMode(vpam::AngleMode m) { testDeg = m == vpam::AngleMode::DEG; }
}
bool setting_complex_enabled = false, setting_edu_steps = false;
int setting_decimal_precision = 6;
uint8_t setting_brightness = numos::display::kSafeDisplayProfile.initialBacklight;
class SettingsApp {
public: static bool loadPersistentState(); static bool savePersistentState();
};
#include "settings.inc"
struct TestSystem {
    bool _filesystemMounted = false;
    struct Display { void setBacklightLevel(uint8_t) {} } _display;
    void startup() {
#include "startup.inc"
    }
};

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string scenario = argv[1];
    auto& vars = vpam::VariableManager::instance();
    testMounted = true;
    vars.setVariable('A', vpam::ExactVal::fromInt(17));
    assert(vars.saveToFlash());
    const auto settings = numos::demo::encodeSettingsRecord(false, true, true, 12);
    testFiles["/settings.dat"] = {settings.begin(), settings.end()};
    if (scenario == "empty-mounted") testFiles.clear();
    if (scenario == "invalid-records") {
        testFiles["/vars.dat"] = {0x33, 0x44};
        testFiles["/settings.dat"] = {0x55, 0x66};
    }
    const auto saved = testFiles;
    vars.resetAll();
    testMounted = false;
    testWrites = testRemoves = testRenames = 0;
    if (scenario == "mount-fail" || scenario == "uninitialized-error" || scenario == "corrupt-error")
        testMountError = ESP_FAIL;
    if (scenario == "other-mount-error") testMountError = 0x105;
    if (scenario == "unavailable-downstream") testUnavailable = true;
    TestSystem app;
    app.startup();
    assert(testRegisters == 1 && testFormats == 0);
    assert(app._filesystemMounted == (testMountError == 0));
    assert(testWrites == 0 && testRemoves == 0 && testRenames == 0);
    assert(testFiles == saved);
    assert(!testDiagnostics.empty());
    if (testMountError) {
        assert(testDiagnostics.find("LittleFS FAIL") != std::string::npos);
        assert(testDiagnostics.find("LittleFS OK") == std::string::npos);
        // An explicit later app retry must also preserve the failed image.
        assert(!LittleFS.begin(false));
        assert(testRegisters == 2 && testFormats == 0 && testFiles == saved);
    }
    if (scenario == "valid") {
        assert(vars.getVariable('A').num == 17);
        assert(!numos::testDeg && setting_complex_enabled && setting_edu_steps);
        assert(setting_decimal_precision == 12);
        assert(testDiagnostics.find("variables loaded") != std::string::npos);
    } else {
        assert(vars.getVariable('A').num == 0);
        assert(numos::testDeg && !setting_complex_enabled && !setting_edu_steps);
    }
    std::printf("PASS %s mount=%d registers=%d formats=%d writes=%d\n",
        argv[1], app._filesystemMounted, testRegisters, testFormats, testWrites);
}
