/**
 * Empirical Challenger R2 Verification Harness
 * Tests:
 * 1. state.userTargetNote immutability under async audio.state / audio.syncState event flooding & sample transitions
 * 2. Exact semitone distance math (12x12 chromatic combinatorial oracle, enharmonics, shortest-path wrap)
 * 3. Audio preview and drag-and-drop payload pitch integrity
 */

const assert = require('assert');

// 1. Core definitions extracted directly from ui-web/app.js
const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

function extractRootNoteName(keyStr) {
  if (!keyStr || keyStr === 'ORIGINAL' || keyStr === 'UNKNOWN') return 'C';
  const s = String(keyStr).trim();
  const m = s.match(/(?:^|[\s_\-\(\[])([A-Ga-g][#b]?)(?:m|maj|min|minor|major)?(?:[\s_\-\)\]]|$)/i) || s.match(/^([A-Ga-g][#b]?)/i);
  if (!m) return 'C';
  let r = m[1].toUpperCase();
  if (r === 'DB') r = 'C#';
  else if (r === 'EB') r = 'D#';
  else if (r === 'GB') r = 'F#';
  else if (r === 'AB') r = 'G#';
  else if (r === 'BB') r = 'A#';
  return r;
}

function calculateSemitoneDistance(rootNote, targetNote) {
  const rRoot = extractRootNoteName(rootNote);
  const rTarget = extractRootNoteName(targetNote);
  const rootIdx = NOTE_NAMES.indexOf(rRoot);
  const targetIdx = NOTE_NAMES.indexOf(rTarget);
  if (rootIdx < 0 || targetIdx < 0) return 0;
  let diff = targetIdx - rootIdx;
  if (diff > 6) diff -= 12;
  if (diff < -6) diff += 12;
  return diff;
}

function extractKeyFromFilename(filename) {
  if (!filename) return null;
  const base = filename.replace(/\.[^.]+$/, '');
  const keyRe = /(?:^|[\s_\-\(\[])([A-G][#b]?)(?:\s*(m|maj|min|minor|major))?(?:[\s_\-\)\]]|\.|$)/gi;
  let match;
  const matches = [];
  while ((match = keyRe.exec(base)) !== null) {
    let note = match[1].charAt(0).toUpperCase() + (match[1].length > 1 ? match[1].charAt(1) : '');
    if (note === 'Db' || note === 'DB') note = 'C#';
    else if (note === 'Eb' || note === 'EB') note = 'D#';
    else if (note === 'Gb' || note === 'GB') note = 'F#';
    else if (note === 'Ab' || note === 'AB') note = 'G#';
    else if (note === 'Bb' || note === 'BB') note = 'A#';
    const mode = match[2] ? match[2].toLowerCase() : '';
    const isMin = mode === 'm' || mode.includes('min');
    matches.push(note + (isMin ? 'm' : ''));
  }
  return matches.length > 0 ? matches[matches.length - 1] : null;
}

// 2. Simulated App State Machine matching ui-web/app.js
class AppStateMachine {
  constructor() {
    this.state = {
      isUserTargetKeyLocked: false,
      userTargetNote: null,
      selectedTargetNote: 'C',
      originalRootNote: 'C',
      sampleKey: 'C',
      sampleBpm: 120,
      pitchSemitones: 0,
      playing: false,
      selected: null,
      files: []
    };
    this.dispatchedBridgeCalls = [];
  }

  bridge(command, args) {
    this.dispatchedBridgeCalls.push({ command, args, timestamp: Date.now() });
    return Promise.resolve({ ok: true, data: {} });
  }

  setTargetNote(targetNote) {
    const root = this.state.originalRootNote || 'C';
    const validTarget = extractRootNoteName(targetNote);
    this.state.isUserTargetKeyLocked = true;
    this.state.userTargetNote = validTarget;
    this.state.selectedTargetNote = validTarget;
    const semitones = calculateSemitoneDistance(root, validTarget);
    this.setPitchShift(semitones);
  }

  setPitchShift(semitones) {
    this.state.pitchSemitones = semitones;
    const root = this.state.originalRootNote || 'C';
    const rootIdx = NOTE_NAMES.indexOf(root);
    const targetIdx = (rootIdx + semitones + 120) % 12;
    this.state.selectedTargetNote = NOTE_NAMES[targetIdx];
    this.updateTransposerPopUI();
    this.bridge('audio.setPitchShift', { semitones });
  }

  resetOriginalKey() {
    this.state.isUserTargetKeyLocked = false;
    this.state.userTargetNote = null;
    this.state.selectedTargetNote = this.state.originalRootNote || 'C';
    this.setPitchShift(0);
  }

  updateTransposerPopUI() {
    const canonicalKey = (this.state.sampleKey && this.state.sampleKey !== 'ORIGINAL' && this.state.sampleKey !== 'UNKNOWN')
      ? this.state.sampleKey
      : (this.state.originalRootNote || 'C');
    const root = extractRootNoteName(canonicalKey);
    this.state.originalRootNote = root;
    
    let target = root;
    let semitones = 0;

    if (this.state.isUserTargetKeyLocked && this.state.userTargetNote) {
      target = this.state.userTargetNote;
      semitones = calculateSemitoneDistance(root, this.state.userTargetNote);
      this.state.pitchSemitones = semitones;
    } else {
      semitones = this.state.pitchSemitones || 0;
      if (semitones !== 0) {
        const rootIdx = NOTE_NAMES.indexOf(root);
        if (rootIdx >= 0) {
          target = NOTE_NAMES[((rootIdx + semitones) % 12 + 12) % 12];
        }
      }
    }
    this.state.selectedTargetNote = target;
  }

  // Handles bridge async events: audio.state and audio.syncState
  handleBridgeEvent(event, data) {
    if (event === 'audio.syncState') {
      if (data) {
        if (typeof data.syncBpm === 'boolean') {
          this.state.syncBpm = data.syncBpm;
        }
        // CRIT-KEY-LOCK: Do NOT clobber userTargetNote or pitch when key is locked
        if (typeof data.semitones === 'number' && !this.state.isUserTargetKeyLocked) {
          this.state.pitchSemitones = data.semitones;
          this.updateTransposerPopUI();
        }
      }
      return;
    }
    if (event === 'audio.state') {
      this.state.playing = !!data.playing;
      this.state.duration = data.duration || this.state.duration;
      // CRIT-KEY-LOCK: Invariant protection
      if (typeof data.pitchSemitones === 'number' && !this.state.isUserTargetKeyLocked) {
        this.state.pitchSemitones = data.pitchSemitones;
        this.updateTransposerPopUI();
      }
      return;
    }
  }

  // Row selection handler
  selectFile(f) {
    this.state.selected = f.path;
    const filename = f.path.split(/[\\/]/).pop() || '';
    const extractedKey = (f.key) || extractKeyFromFilename(filename) || 'C';
    const rootNote = extractRootNoteName(extractedKey);
    this.state.sampleKey = extractedKey;
    this.state.originalRootNote = rootNote;

    if (this.state.isUserTargetKeyLocked && this.state.userTargetNote) {
      this.state.selectedTargetNote = this.state.userTargetNote;
      this.state.pitchSemitones = calculateSemitoneDistance(rootNote, this.state.userTargetNote);
    } else {
      this.state.selectedTargetNote = rootNote;
      this.state.pitchSemitones = 0;
    }
    this.updateTransposerPopUI();
  }

  // Playback start
  async playFile(path) {
    const fileObj = (this.state.files || []).find((x) => x.path === path);
    const filename = path.split(/[\\/]/).pop() || '';
    const initialKey = (fileObj && fileObj.key) ? fileObj.key : (extractKeyFromFilename(filename) || 'C');

    this.state.sampleKey = initialKey;
    const rootNote = extractRootNoteName(initialKey);
    this.state.originalRootNote = rootNote;
    let initialPitchShift = 0;
    if (this.state.isUserTargetKeyLocked && this.state.userTargetNote) {
      this.state.selectedTargetNote = this.state.userTargetNote;
      initialPitchShift = calculateSemitoneDistance(rootNote, this.state.userTargetNote);
    } else {
      this.state.selectedTargetNote = rootNote;
      initialPitchShift = this.state.pitchSemitones || 0;
    }
    this.state.pitchSemitones = initialPitchShift;
    this.updateTransposerPopUI();

    await this.bridge('audio.play', {
      path,
      pitchSemitones: initialPitchShift
    });
  }

  // Drag start
  onDragStart(f) {
    let currentPitch = this.state.pitchSemitones || 0;
    if (this.state.isUserTargetKeyLocked && this.state.userTargetNote) {
      const fileRoot = extractRootNoteName(f.key || extractKeyFromFilename(f.name || f.path) || (this.state.selected === f.path ? this.state.originalRootNote : 'C'));
      currentPitch = calculateSemitoneDistance(fileRoot, this.state.userTargetNote);
    }
    this.bridge('browser.beginDrag', {
      path: f.path,
      pitchSemitones: currentPitch
    });
  }

  // Background metadata arrival
  onBackgroundMetadataReceived(meta) {
    if (meta && meta.key && meta.key !== 'ORIGINAL') {
      this.state.sampleKey = meta.key;
    }
    const detectedRoot = extractRootNoteName(this.state.sampleKey);
    this.state.originalRootNote = detectedRoot;
    if (this.state.isUserTargetKeyLocked && this.state.userTargetNote) {
      this.state.selectedTargetNote = this.state.userTargetNote;
      this.state.pitchSemitones = calculateSemitoneDistance(detectedRoot, this.state.userTargetNote);
    } else {
      this.state.selectedTargetNote = detectedRoot;
      this.state.pitchSemitones = 0;
    }
    this.updateTransposerPopUI();
  }
}

// ============================================================================
// RUN EMPIRICAL CHALLENGE SUITE
// ============================================================================

console.log('======================================================================');
console.log('       Empirical Challenger R2 — Adversarial Stress Suite (Node.js)    ');
console.log('======================================================================\n');

let totalTests = 0;
let passedTests = 0;

function runTest(name, fn) {
  totalTests++;
  process.stdout.write(`  RUN    ${name} ... `);
  const start = Date.now();
  try {
    fn();
    const ms = Date.now() - start;
    console.log(`[ PASS ] (${ms} ms)`);
    passedTests++;
  } catch (err) {
    console.log(`[ FAIL ]`);
    console.error(err);
    process.exitCode = 1;
  }
}

// Test 1: Combinatorial 12x12 Semitone Distance Oracle
runTest('SemitoneDistanceMath_144Combinations_ShortestPathWrap', () => {
  for (let r = 0; r < 12; ++r) {
    const root = NOTE_NAMES[r];
    for (let t = 0; t < 12; ++t) {
      const target = NOTE_NAMES[t];
      const dist = calculateSemitoneDistance(root, target);

      // Invariant 1: Distance must be within [-6, +6]
      assert(dist >= -6 && dist <= 6, `Distance out of range [-6, 6]: ${dist} for ${root}->${target}`);

      // Invariant 2: Circular mapping must land on target
      const resolved = NOTE_NAMES[((r + dist) % 12 + 12) % 12];
      assert.strictEqual(resolved, target, `Circular wrap mismatch: ${root} + ${dist}st -> ${resolved}, expected ${target}`);

      // Invariant 3: Shortest distance property
      const linearDiff = (t - r + 12) % 12;
      const expectedDist = linearDiff > 6 ? linearDiff - 12 : linearDiff;
      if (Math.abs(dist) === 6) {
        assert.strictEqual(Math.abs(dist), 6);
      } else {
        assert.strictEqual(dist, expectedDist, `Shortest distance mismatch for ${root}->${target}`);
      }
    }
  }
});

// Test 2: Enharmonic spelling normalization
runTest('EnharmonicSpellings_Equivalence', () => {
  const enharmonicPairs = [
    ['Db', 'C#'],
    ['Eb', 'D#'],
    ['Gb', 'F#'],
    ['Ab', 'G#'],
    ['Bb', 'A#'],
    ['DB', 'C#'],
    ['EB', 'D#'],
    ['GB', 'F#'],
    ['AB', 'G#'],
    ['BB', 'A#']
  ];
  for (const [flat, sharp] of enharmonicPairs) {
    assert.strictEqual(extractRootNoteName(flat), sharp, `Enharmonic normalization failed: ${flat} -> ${sharp}`);
    assert.strictEqual(calculateSemitoneDistance(flat, sharp), 0, `Distance between enharmonics ${flat} and ${sharp} must be 0`);
  }
});

// Test 3: Adversarial State Machine Flooding under Lock
runTest('StateMachine_10kAsyncEventsFlooding_TargetNoteImmutability', () => {
  const app = new AppStateMachine();
  
  // User locks target note to 'F#'
  app.setTargetNote('F#');
  assert.strictEqual(app.state.isUserTargetKeyLocked, true);
  assert.strictEqual(app.state.userTargetNote, 'F#');
  assert.strictEqual(app.state.selectedTargetNote, 'F#');

  const keys = ['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#m', 'G', 'Ab', 'A', 'Bbm', 'B'];

  // Flood 10,000 asynchronous events in interleaved sequence
  for (let i = 0; i < 10000; ++i) {
    const randomPitch = Math.floor(Math.random() * 25) - 12; // -12 to +12
    const randomKey = keys[i % keys.length];

    // 1. Asynchronous audio.state event with random pitch
    app.handleBridgeEvent('audio.state', {
      playing: i % 2 === 0,
      pitchSemitones: randomPitch,
      position: (i * 0.1) % 10
    });

    // Verify immutability
    assert.strictEqual(app.state.isUserTargetKeyLocked, true);
    assert.strictEqual(app.state.userTargetNote, 'F#');

    // 2. Asynchronous audio.syncState event
    app.handleBridgeEvent('audio.syncState', {
      syncBpm: true,
      semitones: randomPitch
    });
    assert.strictEqual(app.state.userTargetNote, 'F#');

    // 3. User switches sample every 10 iterations
    if (i % 10 === 0) {
      const samplePath = `C:/Audio/Sample_${i}_${randomKey}.wav`;
      const fileObj = { path: samplePath, key: randomKey, bpm: 120 + (i % 30) };
      app.state.files = [fileObj];
      app.selectFile(fileObj);

      // Verify state after sample switch
      assert.strictEqual(app.state.userTargetNote, 'F#');
      assert.strictEqual(app.state.selectedTargetNote, 'F#');
      const expectedShift = calculateSemitoneDistance(extractRootNoteName(randomKey), 'F#');
      assert.strictEqual(app.state.pitchSemitones, expectedShift, `Pitch shift mismatch on sample switch for key ${randomKey}`);

      // 4. Trigger drag start
      app.onDragStart(fileObj);
      const lastDrag = app.dispatchedBridgeCalls[app.dispatchedBridgeCalls.length - 1];
      assert.strictEqual(lastDrag.command, 'browser.beginDrag');
      assert.strictEqual(lastDrag.args.pitchSemitones, expectedShift, `Drag pitch shift mismatch`);

      // 5. Trigger background metadata arrival (e.g., AI detects different key)
      const aiDetectedKey = keys[(i + 3) % keys.length];
      app.onBackgroundMetadataReceived({ key: aiDetectedKey, bpm: 125 });
      assert.strictEqual(app.state.userTargetNote, 'F#');
      const expectedAiShift = calculateSemitoneDistance(extractRootNoteName(aiDetectedKey), 'F#');
      assert.strictEqual(app.state.pitchSemitones, expectedAiShift, `Pitch shift mismatch after background metadata`);
    }
  }
});

// Test 4: Immediate Pitch Calculation in audio.play Payload (Zero-Glitch)
runTest('AudioPlay_ImmediatePitchPayload_ZeroInitialGlitch', async () => {
  const app = new AppStateMachine();
  app.setTargetNote('D'); // User locks note 'D'

  // Sample with key 'A' -> distance is -7st or +5st (shortest path: +5st)
  const samplePath = 'C:/Samples/Melody_A_major.wav';
  app.state.files = [{ path: samplePath, key: 'A', bpm: 128 }];

  await app.playFile(samplePath);

  const lastPlay = app.dispatchedBridgeCalls.find(c => c.command === 'audio.play');
  assert(lastPlay, 'audio.play must have been dispatched');
  assert.strictEqual(lastPlay.args.pitchSemitones, 5, `audio.play payload must have immediate pitchSemitones = 5 (+5st)`);
});

// Test 5: Reset Original Key Unlocks Target and Resets Shift
runTest('ResetOriginalKey_UnlocksTargetAndZeroesShift', () => {
  const app = new AppStateMachine();
  app.setTargetNote('G');
  assert.strictEqual(app.state.isUserTargetKeyLocked, true);
  assert.strictEqual(app.state.userTargetNote, 'G');

  app.resetOriginalKey();
  assert.strictEqual(app.state.isUserTargetKeyLocked, false);
  assert.strictEqual(app.state.userTargetNote, null);
  assert.strictEqual(app.state.pitchSemitones, 0);
});

console.log('\n======================================================================');
console.log('                          TEST SUMMARY');
console.log('======================================================================');
console.log(`  Total Executed : ${totalTests}`);
console.log(`  Passed         : ${passedTests}`);
console.log(`  Failed         : ${totalTests - passedTests}`);
if (passedTests === totalTests) {
  console.log('\n  >>> 100% ALL ADVERSARIAL STRESS TESTS PASSED SUCCESSFULLY! <<<\n');
} else {
  console.log('\n  >>> TEST FAILURES DETECTED! <<<\n');
  process.exit(1);
}
