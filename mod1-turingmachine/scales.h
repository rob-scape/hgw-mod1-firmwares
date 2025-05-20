#ifndef SCALES_H
#define SCALES_H

//  measured PWM values for semitones (C to B over 3 octaves + C)
const int tuningValues[] = {
  0, 11, 15, 19, 23, 27, 31, 35, 40, 44, 48, 53,    // Octave 1
  57, 61, 65, 69, 73, 77, 82, 86, 90, 94, 99, 103,  // Octave 2
  107, 112, 116, 120, 124, 128, 132, 137, 141, 145, 149, 153,
  157  // Extra high C (3 octaves + 1)
};
const int totalNotes = sizeof(tuningValues) / sizeof(tuningValues[0]);

// Index sets for scales, based on tuningValues[]
// These represent note positions from 0–11 and are transposed up later

// For all quantizers below: val = 0–(totalNotes-1)
int quantizeToMajor(int val) {
  const int majorSteps[] = {0, 2, 4, 5, 7, 9, 11}; // C major scale degrees
  int octave = val / 12;
  int semitone = val % 12;

  for (int i = 0; i < 7; i++) {
    if (semitone <= majorSteps[i]) {
      return tuningValues[(octave * 12) + majorSteps[i]];
    }
  }
  return tuningValues[(octave * 12) + 11]; // fallback to B
}

int quantizeToMinor(int val) {
  const int minorSteps[] = {0, 2, 3, 5, 7, 8, 10}; // C natural minor
  int octave = val / 12;
  int semitone = val % 12;

  for (int i = 0; i < 7; i++) {
    if (semitone <= minorSteps[i]) {
      return tuningValues[(octave * 12) + minorSteps[i]];
    }
  }
  return tuningValues[(octave * 12) + 10];
}

int quantizeToPhrygian(int val) {
  const int phrygianSteps[] = {0, 1, 3, 5, 7, 8, 10}; // Phrygian mode
  int octave = val / 12;
  int semitone = val % 12;

  for (int i = 0; i < 7; i++) {
    if (semitone <= phrygianSteps[i]) {
      return tuningValues[(octave * 12) + phrygianSteps[i]];
    }
  }
  return tuningValues[(octave * 12) + 10];
}

#endif
