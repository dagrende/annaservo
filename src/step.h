const byte SERVO_COUNT = 6;
const int SERVO_TICK_DELAY_MS = 15;

class Step {
  public:
    int stepTime;  // tenth of seconds
    byte pos[SERVO_COUNT];
    Step();
    void moveTo();
};
