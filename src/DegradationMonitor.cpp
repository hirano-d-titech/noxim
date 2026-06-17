#include "DegradationMonitor.h"
#include <algorithm>

DegradationMonitor::DegradationMonitor()
  : current_temperature(0.0), accumulated_wear(0.0) {}

void DegradationMonitor::updateState(bool is_active) {
  if (is_active) {
    increaseTemperature();
    increaseWear();
  } else {
    decreaseTemperature();
  }
}

// transient stress (heat) accumulation
void DegradationMonitor::increaseTemperature() {
  current_temperature += 0.1;
  if (current_temperature > 100.0) {
    current_temperature = 100.0;
  }
}

// dissipation of heat
void DegradationMonitor::decreaseTemperature() {
  current_temperature -= 0.05;
  if (current_temperature < 0.0) {
    current_temperature = 0.0;
  }
}

// permanent wear / aging accumulation (irreversible)
void DegradationMonitor::increaseWear() {
  accumulated_wear += 0.0001;
}

// transient temperature delay penalty
int DegradationMonitor::getThermalDelay() const {
  return static_cast<int>(current_temperature * 0.1);
}

// permanent wear delay penalty
int DegradationMonitor::getWearDelay() const {
  return static_cast<int>(accumulated_wear * 3.0);
}

int DegradationMonitor::getCurrentDelay() const {
  // basic delay (0 additional) + thermal + wear
  int base_delay = 0;
  return base_delay + getThermalDelay() + getWearDelay();
}

// transient temperature BER penalty
double DegradationMonitor::getThermalBER() const {
  return current_temperature * 0.000002;
}

// permanent wear BER penalty
double DegradationMonitor::getWearBER() const {
  return accumulated_wear * 0.00002;
}

double DegradationMonitor::getCurrentBER() const {
  // basic BER + thermal BER + wear BER
  double base_ber = 1e-9;
  return base_ber + getThermalBER() + getWearBER();
}

double DegradationMonitor::getCurrentLossRate() const {
  // flit loss happens only after wear passes a certain threshold (3.0)
  double threshold = 3.0;
  if (accumulated_wear > threshold) {
    return 0.001 + (accumulated_wear - threshold) * 0.005;
  }
  return 0.0;
}

