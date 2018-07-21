// Based on FunkyNoise or how to visualize audio data by Stefan Petrick https://gist.github.com/anonymous/29388692c2a030cb8738

// noise related
static uint16_t scale;

// calculate noise matrix
// x and y define the lower left point
void FillNoise() {
  for(int i = 0; i < MATRIX_WIDTH; i++) {
    int ioffset = scale * i;
    for(int j = 0; j < MATRIX_HEIGHT; j++) {
      int joffset = scale * j;
      tempMatrix[i][j] = inoise8(noiseX + ioffset, noiseY + joffset, noiseZ);
    }
  }
}

// calculate noise matrix
// x and y define the center
void FillNoiseCentral() {
  for(int i = 0; i < MATRIX_WIDTH; i++) {
    int ioffset = scale * (i - 8);
    for(int j = 0; j < MATRIX_HEIGHT; j++) {
      int joffset = scale * (j - 8);
      tempMatrix[i][j] = inoise8(noiseX + ioffset, noiseY + joffset, noiseZ);
    }
  }
}


void FunkyNoise1() {
  // zoom controlled by band 5 (snare)
  scale = spectrumByteSmoothed[5] / 4;
  FillNoise();
  for(int i = 0; i < MATRIX_WIDTH; i++) {
    for(int j = 0; j < MATRIX_HEIGHT; j++) {
      leds[XY(i,j)] = 
        CHSV(
      // color controlled by lowest basedrum + noisevalue  
      spectrumByteSmoothed[0] + tempMatrix[i][j],
      255,
      // brightness controlled by kickdrum
      spectrumByteSmoothed[1]);
    }
  }
}

void FunkyNoise2() {
  // zoom factor set by inversed band 0 (63-0)
  // 3 added to avoid maximal zoom = plain color
  scale = 3 + 63 - spectrumByteSmoothed[0] / 4;
  // move one step in the noise space when basedrum is present
  // = slowly change the pattern while the beat goes
  if (spectrumByteSmoothed[0] > 128) noiseZ++;
  // calculate the noise array
  FillNoise();
  // map the noise
  for(int i = 0; i < MATRIX_WIDTH; i++) {
    for(int j = 0; j < MATRIX_HEIGHT; j++) {
      leds[XY(i,j)] = 
        CHSV(
      // hue controlled by noise and shifted by band 0 
      // 40 added to end redish
      40 + tempMatrix[i][j] + spectrumByteSmoothed[0] / 4,
      // no changes here 
      255,
      // brightness controlled by the average of band 0 and 1
      // 20 to ensure a minimum brigness
      // = limiting contrast
      20 + (spectrumByteSmoothed[0] + spectrumByteSmoothed[1]) / 2);
    }
  }
}

void FunkyNoise3() {
  // fix zoom factor
  scale = 50;
  // move one step in the noise space when basedrum is present
  // = slowly change the pattern while the beat goes
  if (spectrumByteSmoothed[0] > 128) noiseZ=noiseZ+10;
  // x any y is defining the position in the noise space
  noiseX = spectrumByteSmoothed[3] / 3;
  noiseY = spectrumByteSmoothed[0] / 3;
  // calculate the noise array
  FillNoise();
  // map the noise
  for(int i = 0; i < MATRIX_WIDTH; i++) {
    for(int j = 0; j < MATRIX_HEIGHT; j++) {
      leds[XY(i,j)] = 
        CHSV(
      // hue controlled by noise and shifted by band 0 
      // 40 added to end redish
      40 + tempMatrix[i][j] + spectrumByteSmoothed[0] / 4,
      // no changes here 
      255,
      // brightness controlled by noise
      tempMatrix[i][j]);
    }
  }
}

void FunkyNoise4() {
  // fix zoom factor
  scale = 200;
  // position in the noise space depending on band 5
  // = change of the pattern
  noiseZ=spectrumByteSmoothed[5] / 2;
  // x scrolling through
  // = horizontal movement
  noiseX = noiseX + 50;
  // y controlled by lowest band
  // = jumping of the pattern
  noiseY = spectrumByteSmoothed[0];
  // calculate the noise array
  FillNoise();
  // map the noise
  for(int i = 0; i < MATRIX_WIDTH; i++) {
    for(int j = 0; j < MATRIX_HEIGHT; j++) {
      leds[XY(i,j)] = 
        CHSV(
      // hue controlled by noise and shifted by band 0 
      // 40 added to end redish
      40 + tempMatrix[i][j] + spectrumByteSmoothed[0] / 4,
      // no changes here 
      255,
      // brightness controlled by noise
      tempMatrix[i][j]);
    }
  }
}

void FunkyNoise5() {
  // dynamic zoom: average of band 0 and 1
  scale = 128-(spectrumByteSmoothed[0]+spectrumByteSmoothed[1])/4;
  // position in the noise space depending on x, y and z
  // z slowly scrolling
  noiseZ++;
  // x static
  // y scrolling 
  noiseY = noiseY + 20;
  // calculate the noise array
  // x any y are this time defining THE CENTER 
  FillNoiseCentral();
  // map the noise
  for(int i = 0; i < MATRIX_WIDTH; i++) {
    for(int j = 0; j < MATRIX_HEIGHT; j++) {
      leds[XY(i,j)] = 
        CHSV(
      // hue controlled by noise and shifted by band 0 
      // 40 added to end redish
      120 + tempMatrix[i][j] * 2,
      // no changes here 
      255,
      // brightness controlled by noise
      tempMatrix[i][j]);
    }
  }
}


/// NOT USED ANYMORE BUT POSSIBLY USEFUL IN THE FUTURE

// Bresenham line algorythm 
void Line(int x0, int y0, int x1, int y1, byte color) {
  int dx = abs(x1-x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1-y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy, e2; 
  for(;;) {  
    leds[XY(x0, y0)] = CHSV(color, 255, 255);
    if (x0 == x1 && y0 == y1) break;
    e2 = 2 * err;
    if (e2 > dy) { 
      err += dy; 
      x0 += sx; 
    } 
    if (e2 < dx) { 
      err += dx; 
      y0 += sy; 
    } 
  }
}

// have an eye on the 7 frequency bands
void DrawAnalyzer() {
  // dim area
  for(int i = 0; i < 7; i++) {
    for(int j = 0; j < 4; j++) {
      leds[XY(i + 9, 15 - j)].nscale8(127);
      ;
    }
  }
  // draw lines according to the band levels 
  for(int i = 0; i < 7; i++) {
    Line(i + 9, 15, i + 9, 15 - spectrumByteSmoothed[i] / 64, (-i) * 15);
  }
}
