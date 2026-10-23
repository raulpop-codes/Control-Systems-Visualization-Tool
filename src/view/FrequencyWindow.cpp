#include "view/FrequencyWindow.hpp"
#include "control/FrequencyResponse.hpp"
#include "control/Stability.hpp"
#include "view/Plot.hpp"
#include "view/Slider.hpp"
#include "view/Button.hpp"
#include "view/Font.hpp"
#include "math/Constants.hpp"

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace std;

namespace {

enum class Tab { Bode, Nichols, Nyquist };

typedef vector<pair<double, double>> Curve;

const sf::Color CURVE_COLOR(90, 190, 240);
const sf::Color WT_COLOR(255, 220, 90);      // gain crossover (wt, gamma_k)
const sf::Color WPI_COLOR(220, 120, 240);    // phase crossover (w_-pi, m_k)
const sf::Color CRITICAL_COLOR(255, 90, 90); // the critical point
const sf::Color GOOD_COLOR(140, 255, 160);
const sf::Color BAD_COLOR(255, 130, 130);
const sf::Color TEXT_COLOR(230, 230, 235);
const sf::Color DIM_TEXT_COLOR(140, 140, 150);

// Layout: the plots on the left, the margins panel in the right-hand corner,
// the K slider along the bottom.
const float PLOT_X = 70.0f, PLOT_W = 760.0f;
const float PANEL_X = 850.0f, PANEL_Y = 60.0f, PANEL_W = 235.0f;

string num(double v, int digits = 4) {
    ostringstream out;
    out << setprecision(digits) << v;
    return out.str();
}

void drawText(sf::RenderWindow& window, const sf::Font& font, const string& s,
              float x, float y, unsigned size, sf::Color color) {
    sf::Text text(font, s, size);
    text.setPosition(sf::Vector2f{x, y});
    text.setFillColor(color);
    window.draw(text);
}

// min/max of the finite values with ~8% room on each side. minSpan keeps a
// flat curve from collapsing the view into a line.
void fitRange(const vector<double>& values, double minSpan, double& lo, double& hi) {
    lo = numeric_limits<double>::max();
    hi = numeric_limits<double>::lowest();
    for(double v : values) if(isfinite(v)){ lo = min(lo, v); hi = max(hi, v); }
    if(lo > hi){ lo = -1.0; hi = 1.0; }
    double span = max(hi - lo, minSpan);
    double center = (lo + hi) / 2.0;
    lo = center - span * 0.54;
    hi = center + span * 0.54;
}

// Liang-Barsky: the part of the segment inside the box, if any. Needed
// because a Nyquist locus of an integrator, or a Nichols curve of a triple
// integrator, runs off to thousands of units -- as pixel coordinates those
// are huge floats, and a line to a point that far away comes out wobbly
// even where it crosses the visible part of the plot.
bool clipSegment(double& x0, double& y0, double& x1, double& y1,
                 double xMin, double xMax, double yMin, double yMax) {
    if(!isfinite(x0) || !isfinite(y0) || !isfinite(x1) || !isfinite(y1)) return false;
    double dx = x1 - x0, dy = y1 - y0;
    double p[4] = {-dx, dx, -dy, dy};
    double q[4] = {x0 - xMin, xMax - x0, y0 - yMin, yMax - y0};
    double t0 = 0.0, t1 = 1.0;
    for(int i = 0; i < 4; i++){
        if(p[i] == 0.0){
            if(q[i] < 0.0) return false;
        } else {
            double r = q[i] / p[i];
            if(p[i] < 0.0){ if(r > t1) return false; t0 = max(t0, r); }
            else          { if(r < t0) return false; t1 = min(t1, r); }
        }
    }
    double nx0 = x0 + t0 * dx, ny0 = y0 + t0 * dy;
    double nx1 = x0 + t1 * dx, ny1 = y0 + t1 * dy;
    x0 = nx0; y0 = ny0; x1 = nx1; y1 = ny1;
    return true;
}

// Plot::drawPolyline, but every segment is first cut to the (slightly
// enlarged) visible box -- see clipSegment().
void drawCurve(const Plot& plot, sf::RenderWindow& window, const Curve& points, sf::Color color) {
    double padX = (plot.getWorldXMax() - plot.getWorldXMin()) * 0.05;
    double padY = (plot.getWorldYMax() - plot.getWorldYMin()) * 0.05;
    for(size_t i = 0; i + 1 < points.size(); i++){
        double x0 = points[i].first,     y0 = points[i].second;
        double x1 = points[i + 1].first, y1 = points[i + 1].second;
        if(clipSegment(x0, y0, x1, y1, plot.getWorldXMin() - padX, plot.getWorldXMax() + padX,
                                       plot.getWorldYMin() - padY, plot.getWorldYMax() + padY))
            plot.drawLine(window, plot.toScreen(x0, y0), plot.toScreen(x1, y1), color);
    }
}

void drawVerticalLine(const Plot& plot, sf::RenderWindow& window, double x, sf::Color color) {
    plot.drawLine(window, plot.toScreen(x, plot.getWorldYMin()), plot.toScreen(x, plot.getWorldYMax()), color);
}

void drawHorizontalLine(const Plot& plot, sf::RenderWindow& window, double y, sf::Color color) {
    plot.drawLine(window, plot.toScreen(plot.getWorldXMin(), y), plot.toScreen(plot.getWorldXMax(), y), color);
}

// Same equal-scale fit as the root locus view: the unit circle (and the
// critical point -1 on it) is always in view, plus the part of the locus
// that stays within a few units of the origin -- the rest of an integrator's
// locus runs off to infinity and would just squash everything else.
void nyquistBounds(const vector<FreqSample>& samples, double K, double plotAspect,
                   double& xMin, double& xMax, double& yMin, double& yMax) {
    double reMin = -1.0, reMax = 1.0, imMin = -1.0, imMax = 1.0;
    for(auto& s : samples){
        ComplexNumber z = s.H * ComplexNumber(K);
        if(!isfinite(z.getReal()) || !isfinite(z.getImag()) || z.magnitude() > 3.0) continue;
        reMin = min(reMin, z.getReal());
        reMax = max(reMax, z.getReal());
        imMin = min(imMin, z.getImag());
        imMax = max(imMax, z.getImag());
    }
    double width  = (reMax - reMin) * 1.3;
    double height = (imMax - imMin) * 1.3;
    if(width < height * plotAspect) width = height * plotAspect;
    else                            height = width / plotAspect;
    double reCenter = (reMax + reMin) / 2.0;
    xMin = reCenter - width / 2.0;
    xMax = reCenter + width / 2.0;
    double imCenter = (imMax + imMin) / 2.0;
    yMin = imCenter - height / 2.0;
    yMax = imCenter + height / 2.0;
}

} // namespace

void showFrequencyResponse(const TransferFunction& plant, double deadTime) {
    sf::RenderWindow window(sf::VideoMode({1100, 740}), "Frequency Response Viewer");
    window.setFramerateLimit(60);
    sf::Font font = loadFont();

    // The frequency range comes from the poles and zeros: nothing to ask the user.
    pair<double, double> range = FrequencyResponse::autoRange(plant, deadTime);
    FrequencyResponse fr(plant, deadTime);
    fr.compute(range.first, range.second);
    const vector<FreqSample>& samples = fr.samples();
    double logMin = log10(range.first), logMax = log10(range.second);

    // The K slider runs up to twice the critical gain when there is one, so
    // the interesting crossing (stable -> unstable) sits mid-slider.
    double kMax = 10.0;
    if(auto kCritical = fr.margins(1.0).criticalGain()) kMax = 2.0 * *kCritical;
    double kMin = kMax * 1e-3;
    double initialK = (1.0 >= kMin && 1.0 <= kMax) ? 1.0 : sqrt(kMin * kMax);

    // Bode: magnitude above phase, sharing the (logarithmic) frequency axis.
    Plot magPlot(PLOT_X, 60.0f, PLOT_W, 270.0f, logMin, logMax, -1.0, 1.0);
    Plot phasePlot(PLOT_X, 375.0f, PLOT_W, 250.0f, logMin, logMax, -1.0, 1.0);
    magPlot.setLogX(true);
    phasePlot.setLogX(true);
    phasePlot.setFixedSteps(0.0, 45.0);
    // Nichols: x = phase in degrees, y = magnitude in dB.
    Plot nicholsPlot(PLOT_X, 60.0f, PLOT_W, 565.0f, -1.0, 1.0, -1.0, 1.0);
    nicholsPlot.setFixedSteps(45.0, 0.0);
    // Nyquist: the complex plane.
    Plot nyquistPlot(PLOT_X, 60.0f, PLOT_W, 565.0f, -1.0, 1.0, -1.0, 1.0);

    Slider slider(PLOT_X, 700.0f, PLOT_W, kMin, kMax, initialK);
    Button bodeButton(70.0f, 12.0f, 80.0f, 30.0f, "Bode");
    Button nicholsButton(160.0f, 12.0f, 90.0f, 30.0f, "Nichols");
    Button nyquistButton(260.0f, 12.0f, 90.0f, 30.0f, "Nyquist");
    Button resetViewButton(740.0f, 12.0f, 90.0f, 30.0f, "Reset view");
    Tab tab = Tab::Bode;

    // --- everything that depends on K, recomputed when the slider moves ---
    double K = initialK;
    Margins margins;
    NyquistVerdict nyquist{};
    bool hasRouth = false;
    RouthResult routh{};
    double phaseAtWt = 0.0, phaseAtWPi = 0.0;
    Curve magCurve, phaseCurve, nicholsCurve, nyquistCurve;

    auto update = [&]() {
        K = slider.getValue();
        margins = fr.margins(K);
        nyquist = fr.nyquist(K);
        // Routh-Hurwitz works on polynomials, and e^{-s*tau} isn't one.
        hasRouth = (deadTime == 0.0);
        if(hasRouth) routh = Stability::routhHurwitz(plant.closedLoopCharacteristicPolynomial(K));
        phaseAtWt  = margins.wt  ? fr.phaseDegAt(*margins.wt)  : 0.0;
        phaseAtWPi = margins.wPi ? fr.phaseDegAt(*margins.wPi) : 0.0;

        // Multiplying H by K is +20*lg K on the magnitude and nothing on the
        // phase; on the Nyquist plane it scales the whole locus.
        double kDb = 20.0 * log10(K);
        magCurve.clear(); phaseCurve.clear(); nicholsCurve.clear();
        nyquistCurve.clear();
        for(auto& s : samples){
            double x = log10(s.w); // the x of a Bode plot is log10(w), not w
            magCurve.push_back({x, s.magDb + kDb});
            phaseCurve.push_back({x, s.phaseDeg});
            nicholsCurve.push_back({s.phaseDeg, s.magDb + kDb});
            ComplexNumber z = s.H * ComplexNumber(K);
            nyquistCurve.push_back({z.getReal(), z.getImag()});
        }
    };

    // "Reset view": fit the current tab to the curves at the current K.
    auto resetView = [&]() {
        double kDb = 20.0 * log10(K);
        vector<double> mag, phase;
        for(auto& s : samples){ mag.push_back(s.magDb + kDb); phase.push_back(s.phaseDeg); }
        switch(tab){
            case Tab::Bode: {
                double lo, hi;
                vector<double> m = mag; m.push_back(0.0); // keep the 0 dB line in view
                fitRange(m, 20.0, lo, hi);
                magPlot.setWorldBounds(logMin, logMax, lo, hi);
                fitRange(phase, 90.0, lo, hi);
                phasePlot.setWorldBounds(logMin, logMax, lo, hi);
                break;
            }
            case Tab::Nichols: {
                double xLo, xHi, yLo, yHi;
                vector<double> p = phase; p.push_back(-180.0); // the critical point (-180, 0 dB)
                vector<double> m = mag;   m.push_back(0.0);
                fitRange(p, 90.0, xLo, xHi);
                fitRange(m, 20.0, yLo, yHi);
                nicholsPlot.setWorldBounds(xLo, xHi, yLo, yHi);
                break;
            }
            case Tab::Nyquist: {
                double xLo, xHi, yLo, yHi;
                nyquistBounds(samples, K, PLOT_W / 565.0, xLo, xHi, yLo, yHi);
                nyquistPlot.setWorldBounds(xLo, xHi, yLo, yHi);
                break;
            }
        }
    };

    // Each tab is fitted the first time it is shown, not all at startup:
    // the Nyquist fit depends on K, which the user may have changed by then.
    bool fitted[3] = {false, false, false};
    auto showTab = [&](Tab t) {
        tab = t;
        if(!fitted[static_cast<int>(t)]){
            resetView();
            fitted[static_cast<int>(t)] = true;
        }
    };
    update();
    showTab(Tab::Bode);

    while(window.isOpen()){
        while(const auto event = window.pollEvent()){
            if(event->is<sf::Event::Closed>()) window.close();

            if(slider.handleEvent(*event, window)) update();

            if(bodeButton.handleEvent(*event, window))    showTab(Tab::Bode);
            if(nicholsButton.handleEvent(*event, window)) showTab(Tab::Nichols);
            if(nyquistButton.handleEvent(*event, window)) showTab(Tab::Nyquist);
            if(const auto* key = event->getIf<sf::Event::KeyPressed>()){
                if(key->code == sf::Keyboard::Key::Num1) showTab(Tab::Bode);
                if(key->code == sf::Keyboard::Key::Num2) showTab(Tab::Nichols);
                if(key->code == sf::Keyboard::Key::Num3) showTab(Tab::Nyquist);
            }
            if(resetViewButton.handleEvent(*event, window)) resetView();

            switch(tab){
                case Tab::Bode:
                    // One frequency axis for both plots: whichever one the
                    // user pans or zooms, the other follows on x.
                    if(magPlot.handleEvent(*event, window))
                        phasePlot.setWorldBounds(magPlot.getWorldXMin(), magPlot.getWorldXMax(),
                                                 phasePlot.getWorldYMin(), phasePlot.getWorldYMax());
                    if(phasePlot.handleEvent(*event, window))
                        magPlot.setWorldBounds(phasePlot.getWorldXMin(), phasePlot.getWorldXMax(),
                                               magPlot.getWorldYMin(), magPlot.getWorldYMax());
                    break;
                case Tab::Nichols: nicholsPlot.handleEvent(*event, window); break;
                case Tab::Nyquist: nyquistPlot.handleEvent(*event, window); break;
            }
        }

        window.clear(sf::Color(15, 15, 20));

        // ---------------- the active tab ----------------
        if(tab == Tab::Bode){
            // --- magnitude ---
            magPlot.drawFrame(window);
            magPlot.beginClip(window);
            magPlot.drawGrid(window);
            magPlot.drawAxes(window); // the 0 dB line
            if(margins.wt)  drawVerticalLine(magPlot, window, log10(*margins.wt),  sf::Color(255, 220, 90, 110));
            if(margins.wPi) drawVerticalLine(magPlot, window, log10(*margins.wPi), sf::Color(220, 120, 240, 110));
            drawCurve(magPlot, window, magCurve, CURVE_COLOR);
            if(margins.wt)  magPlot.drawFilledMarker(window, ComplexNumber(log10(*margins.wt), 0.0), WT_COLOR);
            if(margins.wPi) magPlot.drawFilledMarker(window,
                                ComplexNumber(log10(*margins.wPi), 20.0 * log10(*margins.gainMargin)), WPI_COLOR);
            magPlot.endClip(window);
            magPlot.drawTickLabels(window, &font);

            // --- phase ---
            phasePlot.drawFrame(window);
            phasePlot.beginClip(window);
            phasePlot.drawGrid(window);
            phasePlot.drawAxes(window);
            // -180 degrees (and -180 +- 360 ...): where w_-pi is read off.
            int firstK = static_cast<int>(ceil((phasePlot.getWorldYMin() + 180.0) / 360.0));
            for(int k = firstK; -180.0 + 360.0 * k <= phasePlot.getWorldYMax() && k < firstK + 20; k++)
                drawHorizontalLine(phasePlot, window, -180.0 + 360.0 * k, sf::Color(255, 90, 90, 140));
            if(margins.wt)  drawVerticalLine(phasePlot, window, log10(*margins.wt),  sf::Color(255, 220, 90, 110));
            if(margins.wPi) drawVerticalLine(phasePlot, window, log10(*margins.wPi), sf::Color(220, 120, 240, 110));
            drawCurve(phasePlot, window, phaseCurve, CURVE_COLOR);
            if(margins.wt)  phasePlot.drawFilledMarker(window, ComplexNumber(log10(*margins.wt), phaseAtWt), WT_COLOR);
            if(margins.wPi) phasePlot.drawFilledMarker(window, ComplexNumber(log10(*margins.wPi), phaseAtWPi), WPI_COLOR);
            phasePlot.endClip(window);
            phasePlot.drawTickLabels(window, &font);

            drawText(window, font, "Magnitude [dB]", PLOT_X, 44.0f, 12, sf::Color(170, 200, 240));
            drawText(window, font, "Phase [deg]", PLOT_X, 357.0f, 12, sf::Color(170, 200, 240));
            drawText(window, font, "w [rad/s]", 770.0f, 647.0f, 13, sf::Color(170, 200, 240));
        } else if(tab == Tab::Nichols){
            nicholsPlot.drawFrame(window);
            nicholsPlot.beginClip(window);
            nicholsPlot.drawGrid(window);
            nicholsPlot.drawAxes(window);
            // Critical point: (-180 degrees, 0 dB), repeated every 360 degrees.
            int firstK = static_cast<int>(ceil((nicholsPlot.getWorldXMin() + 180.0) / 360.0));
            for(int k = firstK; -180.0 + 360.0 * k <= nicholsPlot.getWorldXMax() && k < firstK + 20; k++)
                nicholsPlot.drawPoleMarker(window, ComplexNumber(-180.0 + 360.0 * k, 0.0), CRITICAL_COLOR);
            drawCurve(nicholsPlot, window, nicholsCurve, CURVE_COLOR);
            if(margins.wt){
                // gamma_k: horizontal distance from where the curve crosses 0 dB to the critical point
                double critical = -180.0 + 360.0 * round((phaseAtWt + 180.0) / 360.0);
                nicholsPlot.drawLine(window, nicholsPlot.toScreen(phaseAtWt, 0.0),
                                             nicholsPlot.toScreen(critical, 0.0), WT_COLOR);
                nicholsPlot.drawFilledMarker(window, ComplexNumber(phaseAtWt, 0.0), WT_COLOR);
            }
            if(margins.wPi){
                // m_k: vertical distance from where the curve crosses -180 degrees to 0 dB
                double mkDb = 20.0 * log10(*margins.gainMargin);
                nicholsPlot.drawLine(window, nicholsPlot.toScreen(phaseAtWPi, mkDb),
                                             nicholsPlot.toScreen(phaseAtWPi, 0.0), WPI_COLOR);
                nicholsPlot.drawFilledMarker(window, ComplexNumber(phaseAtWPi, mkDb), WPI_COLOR);
            }
            nicholsPlot.endClip(window);
            nicholsPlot.drawTickLabels(window, &font);
            drawText(window, font, "|H| [dB]", PLOT_X, 44.0f, 12, sf::Color(170, 200, 240));
            drawText(window, font, "Phase [deg]", 750.0f, 647.0f, 13, sf::Color(170, 200, 240));
        } else {
            nyquistPlot.drawFrame(window);
            nyquistPlot.beginClip(window);
            nyquistPlot.drawGrid(window);
            nyquistPlot.drawAxes(window);
            Curve circle; // |H| = 1  <=>  0 dB
            for(int i = 0; i <= 180; i++)
                circle.push_back({cos(2.0 * M_PI * i / 180.0), sin(2.0 * M_PI * i / 180.0)});
            drawCurve(nyquistPlot, window, circle, sf::Color(90, 90, 120));
            drawCurve(nyquistPlot, window, nyquistCurve, CURVE_COLOR);
            nyquistPlot.drawPoleMarker(window, ComplexNumber(-1.0, 0.0), CRITICAL_COLOR);
            if(margins.wt)  nyquistPlot.drawFilledMarker(window, fr.at(*margins.wt)  * ComplexNumber(K), WT_COLOR);
            if(margins.wPi) nyquistPlot.drawFilledMarker(window, fr.at(*margins.wPi) * ComplexNumber(K), WPI_COLOR);
            nyquistPlot.endClip(window);
            nyquistPlot.drawTickLabels(window, &font);
            drawText(window, font, "Re", 835.0f, 628.0f, 13, sf::Color(170, 200, 240));
            drawText(window, font, "Im", 30.0f, 44.0f, 13, sf::Color(170, 200, 240));
        }

        // ---------------- tabs, slider, hint ----------------
        bodeButton.draw(window, &font);
        nicholsButton.draw(window, &font);
        nyquistButton.draw(window, &font);
        resetViewButton.draw(window, &font);
        float underlineX = tab == Tab::Bode ? 70.0f : tab == Tab::Nichols ? 160.0f : 260.0f;
        float underlineW = tab == Tab::Bode ? 80.0f : 90.0f;
        sf::RectangleShape underline(sf::Vector2f{underlineW, 3.0f});
        underline.setPosition(sf::Vector2f{underlineX, 44.0f});
        underline.setFillColor(sf::Color(255, 220, 90));
        window.draw(underline);

        slider.draw(window, &font);
        drawText(window, font, "Drag to pan, scroll to zoom, keys 1/2/3 switch view", 380.0f, 20.0f, 12, DIM_TEXT_COLOR);

        // ---------------- corner panel: the margins ----------------
        struct Line { string text; sf::Color color; };
        vector<Line> lines;
        lines.push_back({"K = " + num(K), TEXT_COLOR});
        lines.push_back({"", TEXT_COLOR});
        lines.push_back({"Margins", DIM_TEXT_COLOR});
        lines.push_back({margins.wt ? "wt      = " + num(*margins.wt) + " rad/s" : string("wt      = none"), WT_COLOR});
        lines.push_back({margins.wt ? "gamma_k = " + num(*margins.phaseMargin) + " deg" : string("gamma_k = inf"), WT_COLOR});
        lines.push_back({margins.wPi ? "w_-pi   = " + num(*margins.wPi) + " rad/s" : string("w_-pi   = none"), WPI_COLOR});
        lines.push_back({margins.wPi ? "m_k     = " + num(*margins.gainMargin) + " (" + num(20.0 * log10(*margins.gainMargin), 3) + " dB)"
                                     : string("m_k     = inf"), WPI_COLOR});
        lines.push_back({margins.criticalGain() ? "K_crit  = " + num(*margins.criticalGain()) : string("K_crit  = inf"), TEXT_COLOR});
        lines.push_back({"", TEXT_COLOR});
        lines.push_back({"Closed loop (unity feedback)", DIM_TEXT_COLOR});
        if(nyquist.Nplus > 0)
            lines.push_back({"Margins: n/a (N+ > 0)", DIM_TEXT_COLOR});
        else
            lines.push_back({string("Margins: ") + (margins.stable() ? "STABLE" : "UNSTABLE"),
                             margins.stable() ? GOOD_COLOR : BAD_COLOR});
        lines.push_back({string("Nyquist: ") + (nyquist.marginal ? "MARGINAL" : nyquist.stable ? "STABLE" : "UNSTABLE"),
                         nyquist.stable ? GOOD_COLOR : BAD_COLOR});
        lines.push_back({"  turn " + num(nyquist.deltaDeg, 4) + " deg, need " + num(nyquist.requiredDeg, 3), DIM_TEXT_COLOR});
        lines.push_back({"  N0 = " + to_string(nyquist.N0) + ", N+ = " + to_string(nyquist.Nplus), DIM_TEXT_COLOR});
        if(hasRouth){
            bool routhStable = routh.stable;
            lines.push_back({string("Routh:   ") + (routhStable ? "STABLE" : routh.marginallyStable ? "MARGINAL" : "UNSTABLE"),
                             routhStable ? GOOD_COLOR : BAD_COLOR});
        } else {
            lines.push_back({"Routh:   n/a (dead time)", DIM_TEXT_COLOR});
        }

        const float lineH = 21.0f;
        sf::RectangleShape panel(sf::Vector2f{PANEL_W, lines.size() * lineH + 18.0f});
        panel.setPosition(sf::Vector2f{PANEL_X, PANEL_Y});
        panel.setFillColor(sf::Color(25, 25, 30));
        panel.setOutlineColor(sf::Color(90, 90, 100));
        panel.setOutlineThickness(1.0f);
        window.draw(panel);
        for(size_t i = 0; i < lines.size(); i++)
            drawText(window, font, lines[i].text, PANEL_X + 12.0f, PANEL_Y + 9.0f + i * lineH, 14, lines[i].color);

        // The same verdict as a big label above the panel, like the root
        // locus window's STABLE / UNSTABLE.
        bool verdict = nyquist.stable;
        drawText(window, font, nyquist.marginal ? "MARGINAL" : verdict ? "STABLE" : "UNSTABLE",
                 PANEL_X, 14.0f, 18, nyquist.marginal ? WT_COLOR : verdict ? GOOD_COLOR : BAD_COLOR);

        window.display();
    }
}
