#ifndef BOIDS_GAMESTATE_H
#define BOIDS_GAMESTATE_H

typedef struct {
  int coherenceEnabled;
  int alignmentEnabled;
  int separationEnabled;
  int pauseEnabled;
  int fovEnabled;
} Gamestate;

#endif // BOIDS_GAMESTATE_H
