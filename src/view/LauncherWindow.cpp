#include "view/LauncherWindow.hpp"
#include "view/Button.hpp"
#include "view/TextField.hpp"
#include "view/RootLocusWindow.hpp"
#include "view/TimeResponseWindow.hpp"

#include "math/Matrix.hpp"
#include "math/Polynomial.hpp"
#include "control/TransferFunction.hpp"
#include "control/StateSpace.hpp"
#include "control/Conversions.hpp"
#include "control/Stability.hpp"
#include "control/TimeResponse.hpp"

#include <SFML/Graphics.hpp>
#include <optional>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <stdexcept>

using namespace std;

namespace {

// Window is compact on purpose -- see goToChooseMethod()'s comment for
// why the whole app uses one modest size instead of resizing per screen.
constexpr float WINDOW_W = 560.0f;
constexpr float WINDOW_H = 420.0f;
constexpr float CENTER_X = WINDOW_W / 2.0f;

// Matrices/polynomials up to this order comfortably fit the entry grid at
// the window size above without fields running off the edge. "A handful
// of poles" is already the working assumption elsewhere in this codebase
// (see RootLocus.cpp) -- anything a student would actually type in.
constexpr int MAX_ORDER = 6;

sf::Font loadFont() {
    sf::Font font;
    const char* candidates[] = {
        "C:\\Windows\\Fonts\\arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf"
    };
    for(const char* path : candidates){
        if(font.openFromFile(path)) return font;
    }
    return font;
}

// Parses a field's text strictly -- throws instead of silently defaulting
// to 0, so a typo in one matrix cell doesn't quietly build the wrong
// system. Only reached once onBuildStateSpace()/onBuildTransferFunction()
// have already confirmed no field is flat-out empty (see those for why
// empty is handled separately, before this ever runs).
double parseStrict(const TextField& field) {
    const string& v = field.getValue();
    if(v.empty()) throw invalid_argument("empty field");
    size_t consumed = 0;
    double result = stod(v, &consumed);
    if(consumed != v.size()) throw invalid_argument("not a plain number");
    return result;
}

enum class Screen {
    ChooseMethod,
    ChooseOrder,
    EnterStateSpace,
    EnterTransferFunction,
    MainMenu,
    StabilityResult,
    TimeResponseSetup,
    RootLocusSetup
};

class Launcher {
public:
    void run() {
        sf::RenderWindow window(sf::VideoMode({static_cast<unsigned>(WINDOW_W), static_cast<unsigned>(WINDOW_H)}),
                                 "Control Systems Toolkit");
        window.setFramerateLimit(60);
        windowPtr = &window;
        font = loadFont();

        goToChooseMethod();

        while(window.isOpen()){
            while(const auto event = window.pollEvent()){
                if(event->is<sf::Event::Closed>()) window.close();
                handleEvent(*event, window);
            }

            window.clear(sf::Color(15, 15, 20));
            draw(window);
            window.display();
        }
    }

private:
    sf::Font font;
    sf::RenderWindow* windowPtr = nullptr;
    Screen screen = Screen::ChooseMethod;

    // --- system definition ---
    bool useStateSpace = true;
    int order = 2;      // state-space order, or TF denominator degree
    int numDegree = 1;  // TF numerator degree
    optional<StateSpace> ss;
    optional<TransferFunction> tf;
    string errorMessage;

    // --- widgets for whichever screen is active ---
    vector<Button> buttons;
    vector<TextField> fields;

    // --- state-space entry field layout, so drawMatrixLabels() can line
    //     up "A =", "B =", "C =", "D =" with the boxes goToEnterStateSpace()
    //     placed them at ---
    float aX = 0, aY = 0, bX = 0, bY = 0, cX = 0, cY = 0, dX = 0, dY = 0;

    // --- stability result, cached so StabilityResult can redraw it every
    //     frame without recomputing ---
    RouthResult lastRouth;

    // --- shared by the Step/Impulse setup screen ---
    bool isStep = true;

    void clearWidgets() { buttons.clear(); fields.clear(); }

    void unfocusAllExcept(TextField* keep) {
        for(auto& f : fields) if(&f != keep) f.unfocus();
    }

    // ---------------- screen builders ----------------

    // The whole app shares one sf::RenderWindow, created once at a fixed
    // size (see WINDOW_W/H above) and reused across every screen, rather
    // than resizing per screen -- simpler, and avoids the window visibly
    // jumping size as you click through the flow. This screen in
    // particular used to have two stacked buttons in a much bigger
    // window, wasting most of it; now they sit side by side with the
    // question centered above them.
    void goToChooseMethod() {
        screen = Screen::ChooseMethod;
        errorMessage.clear();
        clearWidgets();
        const float btnW = 200.0f, btnH = 60.0f, gap = 40.0f;
        float startX = CENTER_X - (btnW * 2.0f + gap) / 2.0f;
        buttons.emplace_back(startX, 200.0f, btnW, btnH, "State-space");
        buttons.emplace_back(startX + btnW + gap, 200.0f, btnW, btnH, "Transfer function");
    }

    void goToChooseOrder() {
        screen = Screen::ChooseOrder;
        errorMessage.clear();
        clearWidgets();
        if(useStateSpace){
            fields.emplace_back(CENTER_X - 50.0f, 150.0f, 100.0f, 34.0f, "System order n", to_string(order));
        } else {
            fields.emplace_back(CENTER_X - 120.0f, 150.0f, 100.0f, 34.0f, "Denominator degree n", to_string(order));
            fields.emplace_back(CENTER_X + 20.0f, 150.0f, 100.0f, 34.0f, "Numerator degree m", to_string(numDegree));
        }
        buttons.emplace_back(CENTER_X - 80.0f, 230.0f, 160.0f, 42.0f, "Continue");
        buttons.emplace_back(18.0f, 16.0f, 80.0f, 32.0f, "Back");
    }

    void goToEnterStateSpace() {
        screen = Screen::EnterStateSpace;
        errorMessage.clear();
        clearWidgets();

        const float cellW = 44.0f, cellH = 34.0f;
        aX = 25.0f; aY = 60.0f;
        for(int i = 0; i < order; i++)
            for(int j = 0; j < order; j++)
                fields.emplace_back(aX + j * cellW, aY + i * cellH, cellW - 8.0f, cellH - 12.0f, "", "0");

        bX = aX + order * cellW + 28.0f; bY = aY;
        for(int i = 0; i < order; i++)
            fields.emplace_back(bX, bY + i * cellH, cellW - 8.0f, cellH - 12.0f, "", "0");

        cX = aX; cY = aY + order * cellH + 26.0f;
        for(int j = 0; j < order; j++)
            fields.emplace_back(cX + j * cellW, cY, cellW - 8.0f, cellH - 12.0f, "", "0");

        dX = bX; dY = cY;
        fields.emplace_back(dX, dY, cellW - 8.0f, cellH - 12.0f, "", "0");

        buttons.emplace_back(WINDOW_W - 160.0f, WINDOW_H - 52.0f, 140.0f, 36.0f, "Build System");
        buttons.emplace_back(18.0f, 16.0f, 80.0f, 32.0f, "Back");
    }

    void goToEnterTransferFunction() {
        screen = Screen::EnterTransferFunction;
        errorMessage.clear();
        clearWidgets();

        const float cellW = 50.0f, cellH = 34.0f;
        float denX = 25.0f, denY = 130.0f;
        for(int i = 0; i <= order; i++){
            int power = order - i; // highest power first, same convention main.cpp uses
            fields.emplace_back(denX + i * cellW, denY, cellW - 10.0f, cellH - 12.0f, "s^" + to_string(power), "0");
        }

        float numX = 25.0f, numY = 212.0f;
        for(int i = 0; i <= numDegree; i++){
            int power = numDegree - i;
            fields.emplace_back(numX + i * cellW, numY, cellW - 10.0f, cellH - 12.0f, "s^" + to_string(power), "0");
        }

        buttons.emplace_back(WINDOW_W - 160.0f, WINDOW_H - 52.0f, 140.0f, 36.0f, "Build System");
        buttons.emplace_back(18.0f, 16.0f, 80.0f, 32.0f, "Back");
    }

    void goToMainMenu() {
        screen = Screen::MainMenu;
        errorMessage.clear();
        clearWidgets();
        const float btnW = 200.0f, btnH = 50.0f, gapX = 20.0f, gapY = 20.0f;
        float startX = CENTER_X - (btnW * 2.0f + gapX) / 2.0f;
        buttons.emplace_back(startX, 120.0f, btnW, btnH, "Stability");
        buttons.emplace_back(startX + btnW + gapX, 120.0f, btnW, btnH, "Step Response");
        buttons.emplace_back(startX, 120.0f + btnH + gapY, btnW, btnH, "Impulse Response");
        buttons.emplace_back(startX + btnW + gapX, 120.0f + btnH + gapY, btnW, btnH, "Root Locus");
        buttons.emplace_back(CENTER_X - 140.0f, 120.0f + (btnH + gapY) * 2.0f + 10.0f, 280.0f, 42.0f, "New System");
    }

    void goToStabilityResult() {
        screen = Screen::StabilityResult;
        errorMessage.clear();
        clearWidgets();
        buttons.emplace_back(CENTER_X - 70.0f, WINDOW_H - 52.0f, 140.0f, 36.0f, "Back");
    }

    void goToTimeResponseSetup() {
        screen = Screen::TimeResponseSetup;
        errorMessage.clear();
        clearWidgets();
        fields.emplace_back(CENTER_X - 80.0f, 130.0f, 160.0f, 38.0f, "Simulation length (s)", "10");
        fields.emplace_back(CENTER_X - 80.0f, 210.0f, 160.0f, 38.0f, "Time step dt (s)", "0.01");
        buttons.emplace_back(CENTER_X - 70.0f, 280.0f, 140.0f, 42.0f, "Run");
        buttons.emplace_back(18.0f, 16.0f, 80.0f, 32.0f, "Back");
    }

    void goToRootLocusSetup() {
        screen = Screen::RootLocusSetup;
        errorMessage.clear();
        clearWidgets();
        fields.emplace_back(CENTER_X - 80.0f, 130.0f, 160.0f, 38.0f, "Search K up to", "50");
        fields.emplace_back(CENTER_X - 80.0f, 210.0f, 160.0f, 38.0f, "Initial K (slider start)", "1");
        buttons.emplace_back(CENTER_X - 70.0f, 280.0f, 140.0f, 42.0f, "Run");
        buttons.emplace_back(18.0f, 16.0f, 80.0f, 32.0f, "Back");
    }

    // ---------------- button actions ----------------

    void onChooseOrderContinue() {
        int n = static_cast<int>(fields[0].getValueAsDouble(-1.0));
        if(n < 1 || n > MAX_ORDER){
            errorMessage = "Order must be a whole number from 1 to " + to_string(MAX_ORDER) + ".";
            return;
        }
        if(useStateSpace){
            order = n;
            goToEnterStateSpace();
        } else {
            int m = static_cast<int>(fields[1].getValueAsDouble(-1.0));
            if(m < 0 || m > n){
                errorMessage = "Numerator degree must be from 0 to the denominator degree.";
                return;
            }
            order = n;
            numDegree = m;
            goToEnterTransferFunction();
        }
    }

    // Any blank field here means "I didn't actually enter my system" --
    // rather than nag with an error and leave a half-defined system
    // sitting around, that's treated as a hard stop: the app just closes.
    // (Typos that aren't blank -- like "12a" -- still just show an inline
    // error and let you fix that one field.)
    bool anyFieldEmpty(size_t from, size_t count) const {
        for(size_t i = from; i < from + count; i++)
            if(fields[i].getValue().empty()) return true;
        return false;
    }

    void onBuildStateSpace() {
        if(anyFieldEmpty(0, fields.size())){
            if(windowPtr) windowPtr->close();
            return;
        }
        try {
            Matrix A(order, order), B(order, 1), C(1, order), D(1, 1);
            int idx = 0;
            for(int i = 0; i < order; i++)
                for(int j = 0; j < order; j++)
                    A.at(i, j) = parseStrict(fields[idx++]);
            for(int i = 0; i < order; i++)
                B.at(i, 0) = parseStrict(fields[idx++]);
            for(int j = 0; j < order; j++)
                C.at(0, j) = parseStrict(fields[idx++]);
            D.at(0, 0) = parseStrict(fields[idx++]);

            ss = StateSpace(A, B, C, D);
            tf.reset();
            goToMainMenu();
        } catch(const exception&) {
            errorMessage = "One or more entries isn't a valid number.";
        }
    }

    void onBuildTransferFunction() {
        if(anyFieldEmpty(0, fields.size())){
            if(windowPtr) windowPtr->close();
            return;
        }
        try {
            vector<double> denDesc(order + 1), numDesc(numDegree + 1);
            for(int i = 0; i <= order; i++) denDesc[i] = parseStrict(fields[i]);
            for(int i = 0; i <= numDegree; i++) numDesc[i] = parseStrict(fields[order + 1 + i]);

            vector<double> denAsc(denDesc.rbegin(), denDesc.rend());
            vector<double> numAsc(numDesc.rbegin(), numDesc.rend());

            tf = TransferFunction(Polynomial(numAsc), Polynomial(denAsc));
            ss.reset();
            goToMainMenu();
        } catch(const exception&) {
            errorMessage = "One or more coefficients isn't a valid number.";
        }
    }

    void onStability() {
        lastRouth = ss.has_value() ? Stability::routhHurwitz(ss->characteristicPolynomial())
                                    : Stability::routhHurwitz(tf->denominator());
        goToStabilityResult();
    }

    void onRunTimeResponse() {
        double tFinal = fields[0].getValueAsDouble(-1.0);
        double dt = fields[1].getValueAsDouble(-1.0);
        if(tFinal <= 0.0 || dt <= 0.0 || dt >= tFinal){
            errorMessage = "Simulation length and dt must be positive, with dt smaller than the simulation length.";
            return;
        }
        if(!ss.has_value()) ss = Conversions::toStateSpace(*tf);

        auto samples = isStep ? TimeResponse::step(*ss, tFinal, dt) : TimeResponse::impulse(*ss, tFinal, dt);
        // Opens its own window and blocks until it's closed -- this
        // window just sits idle underneath meanwhile, same as the
        // console menu blocking on the plot window today.
        showTimeResponsePlot(samples, isStep ? "Step Response" : "Impulse Response");
        goToMainMenu();
    }

    void onRunRootLocus() {
        double kMax = fields[0].getValueAsDouble(-1.0);
        double initialK = fields[1].getValueAsDouble(-1.0);
        if(kMax <= 0.0){
            errorMessage = "K search range must be positive.";
            return;
        }
        if(initialK < 0.0 || initialK > kMax){
            errorMessage = "Initial K must be between 0 and the search range.";
            return;
        }
        if(!tf.has_value()) tf = Conversions::toTransferFunction(*ss);

        showRootLocusPlot(*tf, kMax, initialK);
        goToMainMenu();
    }

    // ---------------- event handling ----------------

    void handleEvent(const sf::Event& event, sf::RenderWindow& window) {
        bool clickedAField = false;
        for(auto& f : fields){
            if(f.handleEvent(event, window)){
                clickedAField = true;
                unfocusAllExcept(&f);
            }
        }
        if(!clickedAField){
            if(const auto* mp = event.getIf<sf::Event::MouseButtonPressed>()){
                if(mp->button == sf::Mouse::Button::Left) unfocusAllExcept(nullptr);
            }
        }

        switch(screen){
            case Screen::ChooseMethod:
                if(buttons[0].handleEvent(event, window)){ useStateSpace = true; goToChooseOrder(); return; }
                if(buttons[1].handleEvent(event, window)){ useStateSpace = false; goToChooseOrder(); return; }
                break;

            case Screen::ChooseOrder:
                if(buttons[0].handleEvent(event, window)){ onChooseOrderContinue(); return; }
                if(buttons[1].handleEvent(event, window)){ goToChooseMethod(); return; }
                break;

            case Screen::EnterStateSpace:
                if(buttons[0].handleEvent(event, window)){ onBuildStateSpace(); return; }
                if(buttons[1].handleEvent(event, window)){ goToChooseOrder(); return; }
                break;

            case Screen::EnterTransferFunction:
                if(buttons[0].handleEvent(event, window)){ onBuildTransferFunction(); return; }
                if(buttons[1].handleEvent(event, window)){ goToChooseOrder(); return; }
                break;

            case Screen::MainMenu:
                if(buttons[0].handleEvent(event, window)){ onStability(); return; }
                if(buttons[1].handleEvent(event, window)){ isStep = true; goToTimeResponseSetup(); return; }
                if(buttons[2].handleEvent(event, window)){ isStep = false; goToTimeResponseSetup(); return; }
                if(buttons[3].handleEvent(event, window)){ goToRootLocusSetup(); return; }
                if(buttons[4].handleEvent(event, window)){ ss.reset(); tf.reset(); goToChooseMethod(); return; }
                break;

            case Screen::StabilityResult:
                if(buttons[0].handleEvent(event, window)){ goToMainMenu(); return; }
                break;

            case Screen::TimeResponseSetup:
                if(buttons[0].handleEvent(event, window)){ onRunTimeResponse(); return; }
                if(buttons[1].handleEvent(event, window)){ goToMainMenu(); return; }
                break;

            case Screen::RootLocusSetup:
                if(buttons[0].handleEvent(event, window)){ onRunRootLocus(); return; }
                if(buttons[1].handleEvent(event, window)){ goToMainMenu(); return; }
                break;
        }
    }

    // ---------------- drawing ----------------

    string screenTitle() const {
        switch(screen){
            case Screen::ChooseMethod: return "Choose representation";
            case Screen::ChooseOrder: return useStateSpace ? "State-space: system order" : "Transfer function: degrees";
            case Screen::EnterStateSpace: return "Enter A, B, C, D";
            case Screen::EnterTransferFunction: return "Enter numerator and denominator";
            case Screen::MainMenu: return "What do you want to check?";
            case Screen::StabilityResult: return "Stability";
            case Screen::TimeResponseSetup: return isStep ? "Step response setup" : "Impulse response setup";
            case Screen::RootLocusSetup: return "Root locus setup";
        }
        return "";
    }

    // Left-aligned label at a fixed point -- used wherever text is tied
    // to a specific box (matrix labels, table rows), not to the window
    // center.
    void label(sf::RenderWindow& window, const string& text, float x, float y, unsigned size, sf::Color color) {
        sf::Text t(font, text, size);
        t.setPosition({x, y});
        t.setFillColor(color);
        window.draw(t);
    }

    // Horizontally centered on centerX -- used for the screen title and
    // (deliberately) nothing tied to a specific box, since centering
    // moves the text depending on its own width.
    void drawCentered(sf::RenderWindow& window, const string& text, float centerX, float y, unsigned size, sf::Color color) {
        sf::Text t(font, text, size);
        sf::FloatRect bounds = t.getLocalBounds();
        t.setOrigin({bounds.position.x + bounds.size.x / 2.0f, bounds.position.y});
        t.setPosition({centerX, y});
        t.setFillColor(color);
        window.draw(t);
    }

    void drawMatrixLabels(sf::RenderWindow& window) {
        label(window, "A =", aX - 28.0f, aY - 2.0f, 16, sf::Color(170, 200, 240));
        label(window, "B =", bX - 28.0f, bY - 2.0f, 16, sf::Color(170, 200, 240));
        label(window, "C =", cX - 28.0f, cY - 2.0f, 16, sf::Color(170, 200, 240));
        label(window, "D =", dX - 28.0f, dY - 2.0f, 16, sf::Color(170, 200, 240));
    }

    void drawTFLabels(sf::RenderWindow& window) {
        label(window, "Denominator alpha(s):", 25.0f, 90.0f, 15, sf::Color(170, 200, 240));
        label(window, "Numerator beta(s):", 25.0f, 172.0f, 15, sf::Color(170, 200, 240));
    }

    void drawStabilityResult(sf::RenderWindow& window) {
        float y = 65.0f;
        label(window, "Routh table:", 25.0f, y, 15, sf::Color(200, 200, 210));
        y += 24.0f;

        for(auto& row : lastRouth.table){
            ostringstream line;
            line << fixed << setprecision(2);
            for(double v : row) line << setw(9) << v;
            label(window, line.str(), 25.0f, y, 12, sf::Color(210, 210, 220));
            y += 18.0f;
        }

        y += 12.0f;
        ostringstream rhp;
        rhp << "Right-half-plane roots: " << lastRouth.rightHalfPlaneRoots;
        label(window, rhp.str(), 25.0f, y, 14, sf::Color(210, 210, 220));
        y += 30.0f;

        string verdict = lastRouth.marginallyStable ? "MARGINALLY STABLE" : lastRouth.stable ? "STABLE" : "UNSTABLE";
        sf::Color verdictColor = lastRouth.marginallyStable ? sf::Color(255, 210, 90)
                                : lastRouth.stable ? sf::Color(140, 255, 160) : sf::Color(255, 130, 130);
        label(window, "Verdict: " + verdict, 25.0f, y, 18, verdictColor);
    }

    void draw(sf::RenderWindow& window) {
        drawCentered(window, screenTitle(), CENTER_X, 20.0f, 20, sf::Color(230, 230, 235));

        if(!errorMessage.empty()){
            drawCentered(window, errorMessage, CENTER_X, 50.0f, 14, sf::Color(255, 120, 120));
        }

        switch(screen){
            case Screen::EnterStateSpace: drawMatrixLabels(window); break;
            case Screen::EnterTransferFunction: drawTFLabels(window); break;
            case Screen::StabilityResult: drawStabilityResult(window); break;
            default: break;
        }

        for(auto& f : fields) f.draw(window, &font);
        for(auto& b : buttons) b.draw(window, &font);
    }
};

} // namespace

void runLauncherApp() {
    Launcher app;
    app.run();
}
