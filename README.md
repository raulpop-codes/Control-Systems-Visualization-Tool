# Control Systems Toolkit

A graphical tool for analyzing LTI system stability and behavior --
state-space or transfer function input, then stability (Routh-Hurwitz),
step response, impulse response, or root locus, each with a proper plot
window (root locus with a live, draggable K slider).

## Screenshots

### 1. Choose how to describe the system

<img src="assets/screenshots/choose-representation.png" width="500">

Every analysis starts here: describe the system as state-space matrices,
or as a transfer function's coefficients.

### 2a. Transfer function input

<img src="assets/screenshots/transfer-function-degrees.png" width="500">

First pick the numerator/denominator degrees...

<img src="assets/screenshots/tf-numerator-denominator.png" width="500">

...then enter alpha(s) and beta(s) coefficient by coefficient.

### 2b. State-space input

<img src="assets/screenshots/state-space-order.png" width="500">

For state-space, pick the system order n first...

<img src="assets/screenshots/state-space-abcd.png" width="500">

...then fill in A, B, C, and D.

### 3. Choose what to check

<img src="assets/screenshots/main-menu.png" width="500">

Once the system is built, pick an analysis. "New system" resets back to
step 1 without closing the app.

### 4. Stability

<img src="assets/screenshots/stability-result.png" width="500">

Runs the Routh-Hurwitz table and reports the verdict (stable / marginally
stable / unstable) along with the right-half-plane root count.

### 5. Step response

<img src="assets/screenshots/step-response-setup.png" width="500">

Set the simulation length and time step...

<img src="assets/screenshots/step-response-plot.png" width="500">

...then see y(t) plotted. (Impulse response follows the same two screens.)

### 6. Root locus

<img src="assets/screenshots/root-locus-viewer.png" width="500">

The closed-loop pole trajectories for k in [0, kMax], with a draggable
slider that moves K and redraws the current closed-loop poles live --
color-coded green/red for stable/unstable as you cross a stability
boundary.

## Building & running

*(Fill in with your actual current build steps -- the toolkit's moved to
a fully graphical flow since these were last documented here, so double
check this section still matches your Makefile/project setup before
relying on it.)*

```bash
make main
./main.exe
```
