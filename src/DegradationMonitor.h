#ifndef __DEGRADATIONMONITOR_H__
#define __DEGRADATIONMONITOR_H__

class DegradationMonitor {
public:
  DegradationMonitor();

  // State update called every cycle
  void updateState(bool is_active);

  // Getter methods for penalty values
  int getCurrentDelay() const;
  double getCurrentBER() const;
  double getCurrentLossRate() const;

  // State accessors for debugging/logging
  double getTemperature() const { return current_temperature; }
  double getWear() const { return accumulated_wear; }

private:
  // State variables
  double current_temperature; // Transient stress (thermal)
  double accumulated_wear;    // Irreversible accumulated wear

  // Encapsulated state transition logic
  void increaseTemperature();
  void decreaseTemperature();
  void increaseWear();

  // Encapsulated penalty calculation functions
  int getThermalDelay() const;
  int getWearDelay() const;
  double getThermalBER() const;
  double getWearBER() const;
};

#endif
