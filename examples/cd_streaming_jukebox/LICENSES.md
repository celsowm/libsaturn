# Public-domain music provenance

`cd_streaming_jukebox` ships no third-party recordings with unknown rights.
All three tracks are freely licensed renders (see `audio-src/`), converted
once to build-ready big-endian S16 stereo @ 44100 Hz (CD quality):

- **Ode to Joy** (`audio-src/ode_to_joy.ogg` -> `ode_to_joy.s16be`):
  public-domain MIDI render from the Mutopia Project, converted to Ogg by
  Wikimedia user Raul654, marked public domain (CC Public Domain Mark 1.0):
  <https://commons.wikimedia.org/wiki/File:Ode_to_Joy.ogg>.
  Direct file: <https://upload.wikimedia.org/wikipedia/commons/f/f7/Ode_to_Joy.ogg>.
- **Minuet in G major** (`audio-src/minuet_in_g.mp3` -> `minuet_in_g.s16be`):
  mechanical score render served by Wikimedia:
  <https://upload.wikimedia.org/score/f/p/fp28z21e8wiznes3p2x2g1lmgfayegs/fp28z21e.mp3>.
- **Greensleeves** (`audio-src/greensleeves.ogg` -> `greensleeves.s16be`):
  traditional English folk tune, transcribed and rendered by Wikipedia user
  CambridgeBayWeather, licensed **CC BY 3.0** (attribution below) with GFDL
  as alternative:
  <https://en.wikipedia.org/wiki/File:Greensleeves.ogg>.
  Direct file: <https://upload.wikimedia.org/wikipedia/commons/2/20/Greensleeves.ogg>.

Conversion (documented for reproducibility; these `.s16be` files are the
vendored 44.1 kHz sources. The build deterministically box-filters them to
22.05 kHz before ISO staging, without requiring network access or ffmpeg):

    ffmpeg -i ode_to_joy.ogg -ac 2 -ar 44100 -sample_fmt s16 -f s16be \
      -af "afade=t=in:st=0:d=0.05,afade=t=out:st=38.50:d=0.079955" \
      ode_to_joy.s16be
    ffmpeg -i minuet_in_g.mp3 -ac 2 -ar 44100 -sample_fmt s16 -f s16be \
      -af "loudnorm=I=-16:TP=-1.5:LRA=11,afade=t=in:st=0:d=0.05,afade=t=out:st=12.77:d=0.082245" \
      minuet_in_g.s16be
    ffmpeg -i greensleeves.ogg -ac 2 -ar 44100 -sample_fmt s16 -f s16be \
      -af "afade=t=in:st=0:d=0.05,afade=t=out:st=78.68:d=0.079184" \
      greensleeves.s16be

The short fades keep the in-game loop seam click-free; loudnorm brings the
quiet Minuet render up to full scale without clipping.

## Attribution (CC BY 3.0 — Greensleeves)

"Greensleeves — tune for Greensleeves and What Child Is This?, traditional
English folk song and tune, transcribed by Wikipedia user CambridgeBayWeather",
licensed under CC BY 3.0 (<https://creativecommons.org/licenses/by/3.0>),
via <https://en.wikipedia.org/wiki/File:Greensleeves.ogg>. Adapted for this
example: resampled to 44100 Hz stereo, short loop fades applied.

The underlying compositions are public-domain works:

- **Ode to Joy**, Ludwig van Beethoven, Symphony No. 9, Op. 125 (1824):
  <https://imslp.org/wiki/Symphony_No.9%2C_Op.125_(Beethoven%2C_Ludwig_van)>.
- **Minuet in G major**, BWV Anh. 114, Christian Petzold (1677–1733), formerly
  attributed to J. S. Bach:
  <https://imslp.org/wiki/Minuet_(Pezold%2C_Christian)>.
- **Greensleeves**, traditional English melody, documented in William Ballet's
  lute book (c. 1590–1603):
  <https://imslp.org/wiki/William_Ballet%27s_lute_book%2C_IRL-Dtc_MS_408_(Ballet%2C_William)>.

The PCM encoding, staging scripts, and source code are original to this
repository and fall under the repository license.
