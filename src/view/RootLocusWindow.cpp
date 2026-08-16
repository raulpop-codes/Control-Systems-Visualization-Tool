#include "view/RootLocusWindow.hpp"
#include "control/RootLocus.hpp"
#include "control/Stability.hpp"
#include "view/Plot.hpp"
#include "view/Slider.hpp"

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

using namespace std;

static sf::Font loadFont() {
    sf::Font font;
    const char* candidates[] = {
        "C:\\Windows\\Fonts\\arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf"
    };
    for(const char* path : candidates){
        if(font.openFromFile(path)) return font;
    }
    cerr << "Warning: no font found, labels will not be drawn.\n";
    return font;
}

// Picks a view window around the open-loop poles and zeros -- those are
// the fixed landmarks of the system -- with generous padding to also show
// the nearby locus curvature comfortably.
//
// Deliberately NOT sized to fit every locus branch in full: any system
// with an asymptote (n != m) has a branch that runs off toward infinity as
// k grows (this plant's does -- one closed-loop pole is already past -370
// by K=5), so fitting the whole sweep would zoom the view out to something
// nearly useless just to chase one branch's tail. Instead, that branch is
// simply left to run off the edge of the plot, same as any textbook root
// locus figure -- SFML clips drawing to the window automatically, so nothing
// breaks, it just exits the frame.
static void computeViewBounds(const TransferFunction& plant,
                               double& xMin, double& xMax, double& yMin, double& yMax) {
    double reMin = numeric_limits<double>::max(), reMax = numeric_limits<double>::lowest();
    double imMin = numeric_limits<double>::max(), imMax = numeric_limits<double>::lowest();

    auto expand = [&](const ComplexNumber& z) {
        reMin = min(reMin, z.getReal());
        reMax = max(reMax, z.getReal());
        imMin = min(imMin, z.getImag());
        imMax = max(imMax, z.getImag());
    };
    for(auto& p : plant.poles()) expand(p);
    for(auto& z : plant.zeros()) expand(z);

    if(reMin > reMax){
        // No poles or zeros at all -- shouldn't normally happen, but fall
        // back to something sane rather than an inverted/empty range.
        reMin = -1.0; reMax = 1.0; imMin = -1.0; imMax = 1.0;
    }

    // Enforce a minimum span so tightly-clustered poles/zeros don't zoom
    // in so far that the locus curvature around them is unreadable.
    double reSpan = max(reMax - reMin, 4.0);
    double imSpan = max(imMax - imMin, 4.0);

    double reCenter = (reMax + reMin) / 2.0;
    double imCenter = (imMax + imMin) / 2.0;

    const double padding = 0.6; // 60% extra room on each side
    xMin = reCenter - reSpan * (0.5 + padding);
    xMax = reCenter + reSpan * (0.5 + padding);
    yMin = imCenter - imSpan * (0.5 + padding);
    yMax = imCenter + imSpan * (0.5 + padding);
}

void showRootLocusPlot(const TransferFunction& plant, double kMax, double initialK) {
    sf::RenderWindow window(sf::VideoMode({900, 700}), "Root Locus Viewer");
    window.setFramerateLimit(60);
    sf::Font font = loadFont();

    RootLocus rootLocus(plant);
    vector<RootLocusPoint> locus = rootLocus.compute(kMax, 3000); // precomputed once

    double xMin, xMax, yMin, yMax;
    computeViewBounds(plant, xMin, xMax, yMin, yMax);
    Plot plot(50.0f, 50.0f, 800.0f, 500.0f, xMin, xMax, yMin, yMax);
    Slider slider(50.0f, 610.0f, 800.0f, 0.0, kMax, initialK);

    // Group closed-loop poles into branches by index: locus[i].poles[b] is
    // one point on branch b. Works because RootLocus::compute keeps the
    // same pole ordering from Durand-Kerner across neighboring k values.
    size_t branchCount = locus.empty() ? 0 : locus[0].poles.size();

    vector<ComplexNumber> currentPoles = plant.closedLoopPoles(slider.getValue());
    bool currentlyStable = Stability::routhHurwitz(
        plant.closedLoopCharacteristicPolynomial(slider.getValue())).stable;

    while(window.isOpen()){
        while(const auto event = window.pollEvent()){
            if(event->is<sf::Event::Closed>()) window.close();

            if(slider.handleEvent(*event, window)){
                currentPoles = plant.closedLoopPoles(slider.getValue());
                currentlyStable = Stability::routhHurwitz(
                    plant.closedLoopCharacteristicPolynomial(slider.getValue())).stable;
            }
        }

        window.clear(sf::Color(15, 15, 20));

        plot.drawFrame(window);
        plot.drawAxes(window);

        for(size_t b = 0; b < branchCount; b++){
            vector<ComplexNumber> branch;
            for(auto& point : locus) if(b < point.poles.size()) branch.push_back(point.poles[b]);
            plot.drawPolyline(window, branch, sf::Color(90, 150, 220, 150));
        }

        for(auto& p : plant.poles()) plot.drawPoleMarker(window, p, sf::Color(240, 140, 90));
        for(auto& z : plant.zeros()) plot.drawZeroMarker(window, z, sf::Color(120, 220, 140));

        sf::Color poleColor = currentlyStable ? sf::Color(255, 230, 90) : sf::Color(255, 80, 80);
        for(auto& p : currentPoles) plot.drawFilledMarker(window, p, poleColor);

        slider.draw(window, &font);

        ostringstream status;
        status << (currentlyStable ? "STABLE" : "UNSTABLE");
        sf::Text text(font, status.str(), 18);
        text.setPosition(sf::Vector2f{50.0f, 15.0f});
        text.setFillColor(currentlyStable ? sf::Color(140, 255, 160) : sf::Color(255, 130, 130));
        window.draw(text);

        window.display();
    }
}
