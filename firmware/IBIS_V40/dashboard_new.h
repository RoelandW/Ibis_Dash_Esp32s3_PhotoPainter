// New dashboard drawing code — included inline in drawDashboard()
// This file contains the display loop content for the new design

  display.setFullWindow();
  display.firstPage();

  do {
    feedWatchdog();
    display.fillScreen(GxEPD_WHITE);

    int16_t tx1, ty1;
    uint16_t tw, th;

    // ===== DECORATIVE BORDER =====
    // Outer thick border
    display.drawRect(3, 3, W - 6, H - 6, GxEPD_BLACK);
    display.drawRect(4, 4, W - 8, H - 8, GxEPD_BLACK);
    display.drawRect(5, 5, W - 10, H - 10, GxEPD_BLACK);
    // Inner thin border
    display.drawRect(10, 10, W - 20, H - 20, GxEPD_BLACK);
    // Content area
    int cx = 14, cy = 14, cw = W - 28, ch = H - 28;

    // ===== HEADER BAR =====
    int headerH = 42;
    display.fillRect(cx, cy, cw, headerH, GxEPD_BLACK);

    // Name (left)
    display.setFont(&fonnts_com_Maison_Neue_Bold24pt7b);
    display.setTextColor(GxEPD_WHITE);
    String nameStr = (USER_NAME.length() > 0) ? USER_NAME : "GARMIN";
    nameStr.toUpperCase();
    display.setCursor(cx + 14, cy + 32);
    display.print(nameStr);

    // "GARMIN STATS" (center) - smaller
    display.setFont(&fonnts_com_Maison_Neue_Bold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.getTextBounds("GARMIN STATS", 0, 0, &tx1, &ty1, &tw, &th);
    display.setCursor(cx + (cw - tw) / 2, cy + 28);
    display.print("GARMIN STATS");

    // Year (right) - large red
    display.setFont(&fonnts_com_Maison_Neue_Bold24pt7b);
    display.setTextColor(GxEPD_RED);
    String yearStr = String(dashYear > 0 ? dashYear : 2026);
    display.getTextBounds(yearStr, 0, 0, &tx1, &ty1, &tw, &th);
    display.setCursor(cx + cw - tw - 14, cy + 32);
    display.print(yearStr);

    // ===== TWO COLUMNS =====
    int colY = cy + headerH;
    int colH = ch - headerH;
    int colW2 = cw / 2;
    int leftX = cx;
    int rightX = cx + colW2;

    // Column divider
    display.drawLine(rightX, colY, rightX, cy + ch, GxEPD_BLACK);

    // Color accent bars (5px)
    display.fillRect(leftX, colY, colW2, 5, GxEPD_RED);
    if (dualSport) {
      display.fillRect(rightX + 1, colY, colW2 - 1, 5, GxEPD_GREEN);
    }

    // ===== DRAW SPORT COLUMN =====
    // Lambda to draw one column
    auto drawColumn = [&](int colX, int colWidth, const String& sport, float km, float goal,
                          float dPct, int count, float tHours, float avgSpd, int movSecs, float dist,
                          const String& dateStr, const String& polyline, uint16_t accentColor
#ifndef MAPS_DISABLED
                          , bool hasMap, uint8_t *mapBuf, int mapBufLen, const String& mapMsg
#endif
                          ) {
      int pad = 8;
      int y = colY + 8;  // below accent bar

      // --- TOP SECTION: ring + stats ---
      int ringCX = colX + 44;
      int ringCY = y + 47;
      int ringR = 30;
      int ringThick = 4;

      // Ring background (thin black circles)
      for (int t = 0; t < ringThick; t++) {
        display.drawCircle(ringCX, ringCY, ringR - t, GxEPD_BLACK);
      }
      // Ring progress arc
      if (dPct > 0) {
        float endAngle = dPct * 360.0;
        if (endAngle > 360) endAngle = 360;
        for (float a = -PI/2; a < -PI/2 + endAngle * PI / 180.0; a += 0.015) {
          for (int t = 0; t < ringThick; t++) {
            int px = ringCX + (int)((ringR - t) * cos(a));
            int py = ringCY + (int)((ringR - t) * sin(a));
            display.drawPixel(px, py, accentColor);
          }
        }
      }

      // Percentage text in ring center
      int pctVal = (int)(dPct * 100);
      display.setTextColor(GxEPD_BLACK);
      char pctBuf[8];
      snprintf(pctBuf, sizeof(pctBuf), "%d%%", pctVal);
      display.setFont(&fonnts_com_Maison_Neue_Bold18pt7b);
      display.getTextBounds(pctBuf, 0, 0, &tx1, &ty1, &tw, &th);
      if (tw <= (uint16_t)(ringR * 2 - 14)) {
        display.setCursor(ringCX - tw / 2, ringCY + 8);
        display.print(pctBuf);
      } else {
        display.setFont(&fonnts_com_Maison_Neue_Bold9pt7b);
        display.getTextBounds(pctBuf, 0, 0, &tx1, &ty1, &tw, &th);
        display.setCursor(ringCX - tw / 2, ringCY + 5);
        display.print(pctBuf);
      }

      // Stats to right of ring
      int statsX = colX + 82;
      int statsY = y;
      int colRight = colX + colWidth - pad - 10;

      auto drawInlinePair = [&](int leftX, int rightX, const String& value, const String& label,
                                int baseline, bool rightAlign) {
        int16_t vx1, vy1, lx1, ly1;
        uint16_t vw, vh, lw, lh;
        display.setFont(&fonnts_com_Maison_Neue_Bold18pt7b);
        display.getTextBounds(value, 0, 0, &vx1, &vy1, &vw, &vh);
        display.setFont(&fonnts_com_Maison_Neue_Bold9pt7b);
        display.getTextBounds(label, 0, 0, &lx1, &ly1, &lw, &lh);
        int gap = 3;
        int groupW = vw + gap + lw;
        int x = rightAlign ? rightX - groupW : leftX;
        if (x < leftX) x = leftX;

        display.setFont(&fonnts_com_Maison_Neue_Bold18pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(x, baseline);
        display.print(value);
        display.setFont(&fonnts_com_Maison_Neue_Bold9pt7b);
        display.setCursor(x + vw + gap, baseline);
        display.print(label);
      };

      // Sport name
      display.setFont(&fonnts_com_Maison_Neue_Bold18pt7b);
      display.setTextColor(accentColor);
      display.setCursor(statsX, statsY + 31);
      display.print(getSportTitle(sport));

      // Distance and goal on one line.
      display.setFont(&fonnts_com_Maison_Neue_Bold18pt7b);
      display.setTextColor(GxEPD_BLACK);
      char kmBuf[32];
      if (km >= 1000) snprintf(kmBuf, sizeof(kmBuf), "%.0f", km);
      else snprintf(kmBuf, sizeof(kmBuf), "%.1f", km);
      display.setCursor(statsX, statsY + 64);
      display.print(kmBuf);

      display.getTextBounds(kmBuf, statsX, statsY + 64, &tx1, &ty1, &tw, &th);
      display.setFont(&fonnts_com_Maison_Neue_Bold9pt7b);
      display.setTextColor(GxEPD_BLACK);
      display.setCursor(statsX + tw + 7, statsY + 64);
      char goalBuf[32];
      if (goal > 0) snprintf(goalBuf, sizeof(goalBuf), "KM / %d", (int)goal);
      else snprintf(goalBuf, sizeof(goalBuf), "KM");
      display.print(goalBuf);

      // Count and total time, labels tucked inline to save map height.
      String countStr = String(count);
      drawInlinePair(statsX, colRight, countStr, getActivityLabelFor(sport), statsY + 98, false);

      // Time
      int hrs = (int)tHours;
      int mins = (int)((tHours - hrs) * 60);
      char timeBuf[16];
      snprintf(timeBuf, sizeof(timeBuf), "%dh%02dm", hrs, mins);
      display.setFont(&fonnts_com_Maison_Neue_Bold18pt7b);
      display.setTextColor(GxEPD_BLACK);
      display.getTextBounds(timeBuf, 0, 0, &tx1, &ty1, &tw, &th);
      display.setCursor(colRight - tw, statsY + 98);
      display.print(timeBuf);

      // --- Divider line ---
      int divY = y + 112;
      display.drawLine(colX + pad, divY, colX + colWidth - pad, divY, GxEPD_BLACK);

      // --- MAP SECTION ---
      int mapLabelY = divY + 18;
      display.setFont(&fonnts_com_Maison_Neue_Bold9pt7b);
      display.setTextColor(accentColor);
      display.setCursor(colX + pad, mapLabelY);
      String mapLabel = getLastLabel(sport);
      if (dateStr.length() > 0) mapLabel += " - " + dateStr;
      display.print(mapLabel);

      // Map box with grid
      int mbX = colX + pad;
      int mbY = mapLabelY + 8;
      int mbW = colWidth - 2 * pad;
      int mbH = colY + colH - mbY - 36;  // compact one-line stats below

      display.drawRect(mbX, mbY, mbW, mbH, GxEPD_BLACK);

      // Grid pattern (every 20px)
      for (int gx = mbX + 20; gx < mbX + mbW; gx += 20) {
        for (int gy = mbY + 2; gy < mbY + mbH - 1; gy += 3) {
          display.drawPixel(gx, gy, GxEPD_BLACK);
        }
      }
      for (int gy = mbY + 20; gy < mbY + mbH; gy += 20) {
        for (int gx = mbX + 2; gx < mbX + mbW - 1; gx += 3) {
          display.drawPixel(gx, gy, GxEPD_BLACK);
        }
      }

      // Try downloaded map image first, fall back to polyline
      bool mapRendered = false;
#ifndef MAPS_DISABLED
      String mapStatus = "";
      if (hasMap && mapBuf && mapBufLen > 0) {
        int imgX = mbX + 1 + max(0, (mbW - 2 - MAP_IMAGE_WIDTH) / 2);
        int imgY = mbY + 1 + max(0, (mbH - 2 - MAP_IMAGE_HEIGHT) / 2);
        mapRendered = drawCachedMapImage(imgX, imgY, mapBuf, mapBufLen);
        if (!mapRendered) mapStatus = "decode fail";
      } else if (hasMap) {
        mapStatus = "buf null";
      } else {
        mapStatus = mapMsg.length() > 0 ? mapMsg : "no map";
      }
      if (!mapRendered && mapStatus.length() > 0) {
        display.setFont(&fonnts_com_Maison_Neue_Light9pt7b);
        display.setTextColor(GxEPD_RED);
        display.setCursor(mbX + 3, mbY + mbH - 4);
        display.print(mapStatus);
      }
#endif

      if (!mapRendered && polyline.length() > 0) {
        std::vector<Point> pts = decodePolyline(polyline);
        if (pts.size() >= 2) {
          float mnLa = pts[0].lat, mxLa = pts[0].lat, mnLo = pts[0].lon, mxLo = pts[0].lon;
          for (auto &p : pts) {
            if (p.lat < mnLa) mnLa = p.lat; if (p.lat > mxLa) mxLa = p.lat;
            if (p.lon < mnLo) mnLo = p.lon; if (p.lon > mxLo) mxLo = p.lon;
          }
          float lR = mxLa - mnLa; if (lR < 1e-6) lR = 1e-6;
          float oR = mxLo - mnLo; if (oR < 1e-6) oR = 1e-6;
          int dm = 8;
          int dW = mbW - 2 * dm, dH = mbH - 2 * dm;
          float rA = oR / lR, zA = (float)dW / (float)dH;
          int aW, aH, oX = 0, oY = 0;
          if (rA > zA) { aW = dW; aH = (int)(dW / rA); oY = (dH - aH) / 2; }
          else { aH = dH; aW = (int)(dH * rA); oX = (dW - aW) / 2; }

          for (size_t i = 1; i < pts.size(); i++) {
            int px0 = mbX + dm + oX + (int)(((pts[i-1].lon - mnLo) / oR) * aW);
            int py0 = mbY + dm + oY + (int)((1.0 - (pts[i-1].lat - mnLa) / lR) * aH);
            int px1 = mbX + dm + oX + (int)(((pts[i].lon - mnLo) / oR) * aW);
            int py1 = mbY + dm + oY + (int)((1.0 - (pts[i].lat - mnLa) / lR) * aH);
            display.drawLine(px0 - 1, py0, px1 - 1, py1, GxEPD_BLACK);
            display.drawLine(px0 + 1, py0, px1 + 1, py1, GxEPD_BLACK);
            display.drawLine(px0, py0 - 1, px1, py1 - 1, GxEPD_BLACK);
            display.drawLine(px0, py0 + 1, px1, py1 + 1, GxEPD_BLACK);
            display.drawLine(px0, py0, px1, py1, accentColor);
          }

          // Start dot
          int sx = mbX + dm + oX + (int)(((pts[0].lon - mnLo) / oR) * aW);
          int sy = mbY + dm + oY + (int)((1.0 - (pts[0].lat - mnLa) / lR) * aH);
          display.fillCircle(sx, sy, 5, accentColor);
          display.fillCircle(sx, sy, 2, GxEPD_WHITE);
        }
      }

      // --- BOTTOM STATS ROW ---
      int bsY = mbY + mbH + 5;
      int valueBase = bsY + 22;
      int statX1 = mbX;
      int statX2 = mbX + mbW / 3;
      int statX3 = mbX + 2 * mbW / 3 - 6;
      display.setFont(&fonnts_com_Maison_Neue_Bold18pt7b);
      display.setTextColor(GxEPD_BLACK);

      // Distance
      String bottomVal;
      if (dist > 0) {
        char buf[16]; snprintf(buf, sizeof(buf), "%.1f", dist);
        bottomVal = buf;
      } else { bottomVal = "--"; }
      drawInlinePair(statX1, mbX + mbW - 2, bottomVal, "KM", valueBase, false);

      // Pace/Speed
      if (dist > 0) {
        if (sport == SPORT_RIDE) {
          char buf[16]; snprintf(buf, sizeof(buf), "%.0f", avgSpd);
          bottomVal = buf;
        } else if (dist > 0.01 && movSecs > 0) {
          int ps = (int)(movSecs / dist);
          char buf[12]; snprintf(buf, sizeof(buf), "%d:%02d", ps/60, ps%60);
          bottomVal = buf;
        } else { bottomVal = "--"; }
      } else { bottomVal = "--"; }
      drawInlinePair(statX2, mbX + mbW - 2, bottomVal, sport == SPORT_RIDE ? "KM/H" : "/KM", valueBase, false);

      // Time
      if (movSecs > 0) {
        int tm = movSecs / 60;
        if (tm >= 60) {
          char buf[16]; snprintf(buf, sizeof(buf), "%d:%02d", tm/60, tm%60);
          bottomVal = buf;
        } else {
          char buf[16]; snprintf(buf, sizeof(buf), "%d:%02d", tm, movSecs%60);
          bottomVal = buf;
        }
      } else { bottomVal = "--"; }
      drawInlinePair(statX3, mbX + mbW - 2, bottomVal, "TIME", valueBase, true);
    };

    // Draw left column (sport 1)
    drawColumn(leftX, colW2, SPORT_TYPE, kmDone, YEARLY_GOAL,
               displayPct, activitiesCount, timeHours, lastAvgSpeedKph,
               lastMovingSecs, lastDistKm, lastDateStr, lastPolyline, GxEPD_RED
#ifndef MAPS_DISABLED
               , prefetchedMap1Valid, prefetchedMap1, prefetchedMap1Len, mapDebugMsg1
#endif
               );

    // Draw right column (sport 2) if dual-sport
    if (dualSport) {
      drawColumn(rightX + 1, colW2 - 1, SPORT_TYPE2, kmDone2, YEARLY_GOAL2,
                 displayPct2, activitiesCount2, timeHours2, lastAvgSpeedKph2,
                 lastMovingSecs2, lastDistKm2, lastDateStr2, lastPolyline2, GxEPD_GREEN
#ifndef MAPS_DISABLED
                 , prefetchedMap2Valid, prefetchedMap2, prefetchedMap2Len, mapDebugMsg2
#endif
                 );
    }

  } while (display.nextPage());
