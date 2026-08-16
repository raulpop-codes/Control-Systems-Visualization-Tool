#include "view/TimeResponseWindow.hpp"
#include "view/Plot.hpp"

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>

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

void showTimeResponsePlot(const vector<ResponseSample>& samples, const string& windowTitle) {
    if(samples.empty()){
        cerr << "showTimeResponsePlot: no samples to draw.\n";
        return;
    }

    sf::RenderWindow window(sf::VideoMode({900, 550}), windowTitle);
    window.setFramerateLimit(60);
    sf::Font font = loadFont();

    double tMin = samples.front().t;
    double tMax = samples.back().t;
    if(tMax - tMin < 1e-12) tMax = tMin + 1.0; // guard against a single sample

    double yMin = samples[0].y, yMax = samples[0].y;
    for(auto& s : samples){
        yMin = min(yMin, s.y);
        yMax = max(yMax, s.y);
    }
    // Pad the y range so the curve isn't drawn flush against the frame, and
    // so a perfectly flat response (yMin == yMax, e.g. an all-zero output)
    // still gets a sensible span instead of dividing by zero in Plot.
    double ySpan = yMax - yMin;
    if(ySpan < 1e-9) ySpan = (fabs(yMax) > 1e-9) ? fabs(yMax) : 1.0;
    double yPad = ySpan * 0.1;
    yMin -= yPad;
    yMax += yPad;

    Plot plot(70.0f, 50.0f, 780.0f, 400.0f, tMin, tMax, yMin, yMax);

    vector<pair<double, double>> curve;
    curve.reserve(samples.size());
    for(auto& s : samples) curve.emplace_back(s.t, s.y);

    while(window.isOpen()){
        while(const auto event = window.pollEvent()){
            if(event->is<sf::Event::Closed>()) window.close();
        }

        window.clear(sf::Color(15, 15, 20));

        plot.drawFrame(window);
        plot.drawAxes(window);
        plot.drawPolyline(window, curve, sf::Color(90, 190, 240));

        sf::Text title(font, windowTitle, 20);
        title.setPosition(sf::Vector2f{70.0f, 15.0f});
        title.setFillColor(sf::Color(230, 230, 235));
        window.draw(title);

        ostringstream axisLabel;
        axisLabel << "t: 0 -> " << fixed << setprecision(2) << tMax
                   << "   y: " << setprecision(3) << (yMin + yPad) << " -> " << (yMax - yPad);
        sf::Text axisText(font, axisLabel.str(), 15);
        axisText.setPosition(sf::Vector2f{70.0f, 460.0f});
        axisText.setFillColor(sf::Color(170, 170, 180));
        window.draw(axisText);

        window.display();
    }
}
