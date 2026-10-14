// Web Audio API Authentic Chess Sound System
// High-fidelity, zero-latency, deterministic chess piece audio
// Embedded base64 audio buffers for 100% offline and consistent playback

const SOUND_BASE64 = {
  move: 'SUQzAwAAAAAAc1RJVDIAAAAZAAAAV29vZGVuIHBpZWNlIC0gc2hhcnAgaGl0VFhYWAAAACgAAABDb3B5cmlnaHQAQ29weXJpZ2h0IDIwMDAsIFNvdW5kZG9ncy5jb21UWFhYAAAAFAAAAFNvZnR3YXJlAEF3QysrIHYyLjH/+5DEAAAAAAAAAAAAAAAAAAAAAABYaW5nAAAADwAAAAkAAAl2AA0NDQ0NDQ0NDQ0NGhoaGhoaGhoaGhpqampqampqampqapiYmJiYmJiYmJiYs7Ozs7Ozs7Ozs7POzs7Ozs7Ozs7OzuXl5eXl5eXl5eXl8vLy8vLy8vLy8vL//////////////wAAAFBMQU1FMy4xMDAEuQAAAAAAAAAAFSAkBIVBAAHgAAAJdlcVHFwAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD/+xDEAAPAAAH+AAAAIAAAP8AAAARpJyyJ2uyIEQh5znPaRkYJgDAGAcEw2KwvBYOHjKeeeynnj8RYtiuIsWB4PD+e7p/shhhlCAfk556VPPb1G4NhLQWKj8B8FfAvIzh5cz7/Uv0X///7EMQpg8AAAf4AAAAgAAA/wAAABP/oz6D89TV0lAAEDLBIRGG0ag6AAoAcYAAEJhPBkGMgUoYoobZiUpDGCSBCYIIDZmUDwG0CpKbSaD5mLn7BYBoBAWGbuP+YiYOZqqnfGNOOoHEA//uwxFMAC4ElGbSVAAQWp+WrPcIAwSAzTwmN5UI3NDjUogTQTkYabCGSepg8Nix0MCCMycRGbs3nJGX0Mki0wyAVhDUIrrEQTEQMZuwxpjyRcxeGTB4PAwfWSWbSWC4NQ+gwwWBmUO/HG8iEPBATUg7TNGIMMQ2Z66SV0lay2aenFLKlfFzIfgpzE63n97aRDVpsNQrfefjhhnrmUN3lpoB1ro4S9gfLzwxK7LolDUDSzmedBew/e76YiuIEe+PWZf3Ufn3eZAhKiX0k1ZyemMIgAAAnZ/8DEZO0YyRDJAvA1Q0TkQzKhOFgyzjJAj0WRuLbF2kFgsMAbwapODKigRZozLF0nzAyMS0QhAhvkNPGyBiQ0yWVpiRFFJRkamCjZ2NkUUUVF1A2OFZA+yJ1kUKLqKZaL9I+bGCRq5qbIPa2kcQQdBRgcTdqi7XTqZNBTponDI4lpspNkklKW1u2iiadmsXMJggAyz3UAUge4yDAyA5Q6hI5EkiWk6OAQDLmRDclsTsrxalEYg7yxHVHZLwOC6KvIhM5KvKeRFMENKqRTmGgUi+xmMkb8GQWTRSokMHEAIWagJ1iZsvOOs79I/VuLxaSu9UiMi/5wj/L/BKDFAmO1x8gYowXvvYwZF7EjCDfYFKmCKOhCAJMNDZBCjwGAMigSkYcx5GYAkq45Jp5ad5p69jIyZAik/jplRqMoqxNyPKoi9RYYWsQ83arDq6CNQjCw+GSeJhqUdExFd7vF38+s0jdyzQ1y/a1t+tN/H6/5Yw5ixsYQhAVR2v3+jAOIho4DCNjGQFDWnBc6bDQ19pCoDWSxWWL+dkJCFI1MKX/+4DE0IASST9DXZiAIcam57WEjiQbxI1RywYDCE6wbTqJXUlGtszGdE86o1tLYknPm36EpnnM4ffDXrN2+E8aetQsSmbwMlIiDaJpZgrKHf/QHiTfv928ZAB8eNiHGrebQIUkMTKdgt0W1R5TJhyVtTgeISSDYE6o4kDKxE8kmWijApLwokBEJL0xN0SGEjApLLO3OgmIhmM71V1ZwabpRt1Rm/f5bfqwPrd//9o5Tr/+UBzEkeId9voBVJnCwBI7whQrZEvShMOOUWmSjUyftoEfisNSl4JQxoaXLq9zEAi0qWZnMp3Im5vH+2Ha5qs9L/sNd1G////9t//QTr7P9FV4ZkmIh32+YAACIKWJLJuzhNIsIQeLLNcLUgVTdYq/oNTZESgUFtQORTjrdeBykM3SSZqrtjKhFEaMpXflLf/Htl////6Pp///+vb///aIrBbiEMIjSJ7ROgFX+lKih46ISAYFCeVxlMgTbP/7UMT6gA5hNzWssQ7hm5fmPYSZ1ErXK1Xp5yBIbZqdkZZse2b///9/////LjG/+2//bf//+qxqdvmHQ9+ly5QK0AABOwWlOwoIc8ZAUoGXieMSqyn/+tKzrmBs9FkknpI7dv/6G1b/6f/t///8dWX/q2Eb8rMAUmoCJhXo9H//9OX//////////Xm//wxMQU1FMy4xMDCqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqr/+1DE6wBMGO8nrBhRYTmUZT2DCiSqqqpMQU1FMy4xMDCqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq//tAxPCASk1TJ+wkTWEFrGKxgxVAqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqr/+xDE9wDG7WsTKYBagGIw4hQgCjiqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqv/7EMTWA8AAAf4AAAAgAAA0gAAABKqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq',
  capture: 'SUQzAwAAAAAAc1RJVDIAAAAZAAAAV29vZGVuIHBpZWNlIC0gc2hhcnAgaGl0VFhYWAAAACgAAABDb3B5cmlnaHQAQ29weXJpZ2h0IDIwMDAsIFNvdW5kZG9ncy5jb21UWFhYAAAAFAAAAFNvZnR3YXJlAEF3QysrIHYyLjH/+5DEAAAAAAAAAAAAAAAAAAAAAABYaW5nAAAADwAAAA8AAAzrAAkJCQkJCS4uLi4uLi5OTk5OTk5zc3Nzc3NzmJiYmJiYmKioqKioqLa2tra2tra/v7+/v7+/yMjIyMjI0dHR0dHR0dvb29vb29vk5OTk5OTt7e3t7e3t9vb29vb29v///////wAAAFBMQU1FMy4xMDAEuQAAAAAAAAAAFSAkAkBBAAHgAAAM6zhVl6wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD/+xDEAAPAAAH+AAAAIAAAP8AAAAT//////9CV2skrlmusiYBkIZBGABZhQRiEOTnKSMRh/IxSc/1pppkmPce5LunMCQEYC8CMDCDkHm5gXCQLhcQmA9CQHoShoxmXx6Fw0ZMvl8vm7f/7kMQpg8AAAf4AAAAgGgBigAAAAE003t/QQbqQKZgFhUg3/V/q1f1n9RwhQHwsrDGAIDAAAABFgoDpOh/AAKPwwIBIx4LAzMIgxTAQxOJNMAaAUQAqdDQEZeGaYvkCDgrBwAGkwGmaZAmYSsmNBoLkGQWaVQZi8lGqJUBsoBhOLDtGA1YdAF9jShMPFx80e6jJQ7BwHL1zHmDQOX9MaksxuCzAQSJCWYECyMgKBj6tuq+WmMA+ZfKIsIjK41MLgcxkB0hlLoKbajX477lq4dyXsBMDhUFCeXyhuyaJa0aDVBKecwa/Y77eJDgUBoRtPfSQTUf5NYRvD+fz/1zUb7jHLFtllO/EXuUk59DKZdft3Mec/v/+l7rvnr279O79/7kujFyK5d1FuqUSBEBAAAAAljWNkAAAA2aFUwlQ47hOMwGCUyiToYMYydTw3Ig8xWC4y7RQKjoKgWa7LqYjDMYaEkYlkiYKlIYSkkcFKSZ6lsZ5w4FDvMuBoMVkXMnw+NIjXMug5MjjNMQQWMBwUCoSmRoglngqLZVJcxRAMmARW//7gMTsAAz8+Ru1hoAD4qdoNzvEQMwgFAaL5Q5MMKjwCiGMAwHEYFAwEjB4DVg3BEYRqdMCRylQqE7H0Ai67kosEg2EQVv3bTBKwUMiSyMQAgAIAFYMqcpEmAI4mJAIiICiYE1GyQATAQCVNHjmVtwbHaFpMF4XI7DEy30B5yPl6Jf/LO7PbeacyiVGSL6cz98AEmnteKYAAABiVKlhk+xlxR+l4khDAcTNoPNpGC34xjUiHmWNGFPm9ZA4yOogaKgIDDhjhM1gIxMJTEBRARtFhUnsYlLxpgpMSKpSARUAIAgEwWLIQxEsAgvUHDBBh5hgUmLAOyAHB8VDIKAydVCYICIYXAMD0NX1RYMAgQwqECIDwBEqIYA7ws8eZcCgTyhweiceZU7wjAC7r1m7Es8Yjbpu3cd3KfX7pssqYiJ2twMtyxLY/6L/H1oAAF3tOutEAAACiow+nTGLCMcq4yUbQSHTgaZPOaoy/AjO//uQxOeAHlzdLbnegALnGKcfNcBIZcMchgyQCzyEsNSBYzq+DRQ7AwpMepg2k7lhz+YZNemvDoIFTQS86yAKIE27sNcxTjCMDIoKPktTFQUOaDEgBWAuoITYCgC5WXGAhg8YL5MuLDKBoBEiNSCQIMASKCRkkangBiNJAubE3JTUEQIYQGBwQ/9Gjc10VFxYZdlxaugoIwDdyXYpfB7uQ8xCTNZdmAZbQPDGJdlnB0efqPQ2/8v53G9SSyd7Wy/Khpc7ZIOvfbQeggADb/wDIgKYs+k4DhhDBYRkKLGHhLVQqYChkqVjzssgFsM4mS2lG5LmQ3uSeb9QYVbeWMepkzPbV1iVlctYzTedY///xm0NwR7dPTVK//P//lq+VsZ16yNdIdLRJvvU82N7/z//6/NYUHWYuQpxSk04oAA1N/gAIgsGLip0lwVcqCo6sBStFUKiWFb1uTZWuLDMFl0Cwt2WkrFnDiMeUZOXM2ATtIzMnFB0dMXXnHTUHL/SCGC0PUsn48VN/qhqh1ZvnDa6CQXCP7f/XEQYYASd9IAEPlMF//uQxNcAGqTrMPnNgAHBn6frsPAEWGU4VFXeNGxQIqpSYL4pRI+uueqAeJpYPy4rVaWTHkubd2vc/q4mqWn61C2pbs8/YdrRzUa5KXYnpWFuMSCXVThkVSQEST4AAACMGZj5xoBZcs42ArAlq+xZpqxNjkLEqjKSYfJagIwZVZSLNe/HPAKkqFyrpe0R1D1pWqPOm470Yc7n1tusC7hMSHciqMlV4h3JRiwWTLga8nupspzEBpsyHcYHxI6W4aOW5sYFiUY42lCMNUojUkxBTUWqqj7OmQBDEOQRVAhF0o0yULDHDsmYLOlgDNi5Cq8KvPgR5/Oq92tEqDPBGUWxBBfF5fBwrrhCassS9Z4NWf////////////////RVTEFNRTMuMTAwVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVWJEEAAKHGcACCkIlBJlNAUAZMsqDpMQU1FMy4xMDCqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq//tAxPoAC5i1OawZEqkrkuU1hhmsqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqoNf8kxBTUUzLjEwMKqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqTEFNRTMuMTAwqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqr/+zDE9oBILGsn7D0qaNoLJDWGIYyqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqpMQU1FMy4xMDCqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqkxBTUUzLjEwMKqqqqr/+xDE+wDEjCcTDGEkqJMEYiGHsBSqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqTEFNRTMuMTAwqqqqqv/7EMTggcHADRKMGSAoG4CiYLAABKqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqpMQU1FMy4xMDCqqqqq//sQxNeDwFgBDgAAACgAAD/AAAAEqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqr/+xDE1gPAAAH+AAAAIAAAP8AAAASqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqv/7EMTWA8AAAf4AAAAgAAA/wAAABKqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq//sQxNYDwAAB/gAAACAAAD/AAAAEqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqr/+xDE1gPAAAH+AAAAIAAAP8AAAASqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqv/7EMTWA8AAAf4AAAAgAAA/wAAABKqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq',
  confirmation: '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'
};

let audioCtx = null;
let soundEnabled = true;
let masterVolume = 0.8;
const cachedBuffers = {};
let isDecodingStarted = false;

try {
  const saved = localStorage.getItem('pawngo_sound');
  if (saved !== null) {
    soundEnabled = saved === 'true';
  }
  const savedVol = localStorage.getItem('pawngo_volume');
  if (savedVol !== null) {
    const parsed = parseFloat(savedVol);
    if (!isNaN(parsed) && parsed >= 0 && parsed <= 1) {
      masterVolume = parsed;
    }
  }
} catch (e) {}

function getAudioContext() {
  if (!audioCtx) {
    const AudioContextClass = typeof window !== 'undefined' ? (window.AudioContext || window.webkitAudioContext) : null;
    if (AudioContextClass) {
      audioCtx = new AudioContextClass();
    }
  }
  return audioCtx;
}

function base64ToArrayBuffer(base64) {
  const binaryString = window.atob(base64);
  const len = binaryString.length;
  const bytes = new Uint8Array(len);
  for (let i = 0; i < len; i++) {
    bytes[i] = binaryString.charCodeAt(i);
  }
  return bytes.buffer;
}

async function preloadBuffers(ctx) {
  if (isDecodingStarted || !ctx) return;
  isDecodingStarted = true;

  for (const [name, b64] of Object.entries(SOUND_BASE64)) {
    try {
      const arrayBuffer = base64ToArrayBuffer(b64);
      // decodeAudioData consumes the arrayBuffer, so passing directly is safe
      ctx.decodeAudioData(arrayBuffer, (decoded) => {
        cachedBuffers[name] = decoded;
      }, (err) => {
        console.warn('[Audio] Failed to decode sound: ' + name, err);
      });
    } catch (e) {
      console.warn('[Audio] Decode exception: ' + name, e);
    }
  }
}

// Global unlock listener on first user interaction
if (typeof window !== 'undefined') {
  const unlockAudio = () => {
    const ctx = getAudioContext();
    if (ctx) {
      if (ctx.state === 'suspended') {
        ctx.resume().catch(() => {});
      }
      preloadBuffers(ctx);
      window.removeEventListener('click', unlockAudio, true);
      window.removeEventListener('keydown', unlockAudio, true);
      window.removeEventListener('touchstart', unlockAudio, true);
      window.removeEventListener('mousedown', unlockAudio, true);
    }
  };
  window.addEventListener('click', unlockAudio, true);
  window.addEventListener('keydown', unlockAudio, true);
  window.addEventListener('touchstart', unlockAudio, true);
  window.addEventListener('mousedown', unlockAudio, true);
}

function withRunningContext(callback) {
  if (!soundEnabled) return;
  const ctx = getAudioContext();
  if (!ctx) return;

  preloadBuffers(ctx);

  if (ctx.state === 'suspended') {
    ctx.resume().then(() => {
      callback(ctx);
    }).catch(() => {});
  } else {
    callback(ctx);
  }
}

function playSample(name, volume = 1.0, delaySec = 0) {
  withRunningContext((ctx) => {
    const finalVolume = Math.max(0, Math.min(1, volume * masterVolume));
    if (finalVolume <= 0.0001) return;

    const buffer = cachedBuffers[name];
    if (buffer) {
      try {
        const source = ctx.createBufferSource();
        source.buffer = buffer;
        const gainNode = ctx.createGain();
        gainNode.gain.setValueAtTime(finalVolume, ctx.currentTime);
        source.connect(gainNode);
        gainNode.connect(ctx.destination);
        source.start(ctx.currentTime + delaySec);
        return;
      } catch (e) {
        console.warn('[Audio] Playback failed for ' + name, e);
      }
    }

    // Fallback if not yet decoded: decode on the fly
    if (SOUND_BASE64[name]) {
      try {
        const arrayBuf = base64ToArrayBuffer(SOUND_BASE64[name]);
        ctx.decodeAudioData(arrayBuf, (decoded) => {
          cachedBuffers[name] = decoded;
          const source = ctx.createBufferSource();
          source.buffer = decoded;
          const gainNode = ctx.createGain();
          gainNode.gain.setValueAtTime(finalVolume, ctx.currentTime);
          source.connect(gainNode);
          gainNode.connect(ctx.destination);
          source.start(ctx.currentTime + delaySec);
        });
      } catch (e) {}
    }
  });
}

export function setSoundEnabled(enabled) {
  soundEnabled = enabled;
  try {
    localStorage.setItem('pawngo_sound', enabled ? 'true' : 'false');
  } catch (e) {}
}

export function isSoundEnabled() {
  return soundEnabled;
}

export function setMasterVolume(vol) {
  masterVolume = Math.max(0, Math.min(1, vol));
  try {
    localStorage.setItem('pawngo_volume', masterVolume.toString());
  } catch (e) {}
}

export function getMasterVolume() {
  return masterVolume;
}

// 1. Move sound: authentic, consistent wooden piece tap
export function playMoveSound() {
  playSample('move', 0.9);
}

// 2. Capture sound: crisp, impactful piece capture
export function playCaptureSound() {
  playSample('capture', 0.95);
}

// 3. Castle sound: confirmation chime / double slide
export function playCastleSound() {
  playSample('confirmation', 0.85);
}

