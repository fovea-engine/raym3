#!/usr/bin/env bun
/**
 * Generates text_layout_goldens.json from pretext using deterministic measureWidth.
 * Reference: pretext/src/layout.test.ts
 *
 * Usage:
 *   bun scripts/generate-text-layout-goldens.mjs          # write fixtures
 *   bun scripts/generate-text-layout-goldens.mjs --check  # fail if stale
 */

import { createHash } from 'crypto'
import { readFileSync, writeFileSync, existsSync } from 'fs'
import { dirname, join } from 'path'

const scriptDir = dirname(import.meta.path)
const raym3Root = join(scriptDir, '..')
const pretextRoot = join(raym3Root, '..', 'pretext')
const outPath = join(raym3Root, 'tests', 'fixtures', 'text_layout_goldens.json')

const FONT = '16px Test Sans'
const LINE_HEIGHT = 19
const FONT_SIZE = 16

const emojiPresentationRe = /\p{Emoji_Presentation}/u
const punctuationRe = /[.,!?;:%)\]}'"”’»›…—-]/u
const decimalDigitRe = /\p{Nd}/u
const graphemeSegmenter = new Intl.Segmenter(undefined, { granularity: 'grapheme' })

function measureWidth(text, fontSize = FONT_SIZE) {
  let width = 0
  let previousWasDecimalDigit = false
  for (const ch of text) {
    if (ch === ' ') {
      width += fontSize * 0.33
      previousWasDecimalDigit = false
    } else if (ch === '\t') {
      width += fontSize * 1.32
      previousWasDecimalDigit = false
    } else if (emojiPresentationRe.test(ch) || ch === '\uFE0F') {
      width += fontSize
      previousWasDecimalDigit = false
    } else if (decimalDigitRe.test(ch)) {
      width += fontSize * (previousWasDecimalDigit ? 0.48 : 0.52)
      previousWasDecimalDigit = true
    } else if (isWideCharacter(ch)) {
      width += fontSize
      previousWasDecimalDigit = false
    } else if (punctuationRe.test(ch)) {
      width += fontSize * 0.4
      previousWasDecimalDigit = false
    } else {
      width += fontSize * 0.6
      previousWasDecimalDigit = false
    }
  }
  return width
}

function isWideCharacter(ch) {
  const code = ch.codePointAt(0)
  return (
    (code >= 0x4e00 && code <= 0x9fff) ||
    (code >= 0x3400 && code <= 0x4dbf) ||
    (code >= 0xf900 && code <= 0xfaff) ||
    (code >= 0x2f800 && code <= 0x2fa1f) ||
    (code >= 0x20000 && code <= 0x2a6df) ||
    (code >= 0x2a700 && code <= 0x2b73f) ||
    (code >= 0x2b740 && code <= 0x2b81f) ||
    (code >= 0x2b820 && code <= 0x2ceaf) ||
    (code >= 0x2ceb0 && code <= 0x2ebef) ||
    (code >= 0x2ebf0 && code <= 0x2ee5d) ||
    (code >= 0x30000 && code <= 0x3134f) ||
    (code >= 0x31350 && code <= 0x323af) ||
    (code >= 0x323b0 && code <= 0x33479) ||
    (code >= 0x3000 && code <= 0x303f) ||
    (code >= 0x3040 && code <= 0x309f) ||
    (code >= 0x30a0 && code <= 0x30ff) ||
    (code >= 0x3130 && code <= 0x318f) ||
    (code >= 0xac00 && code <= 0xd7af) ||
    (code >= 0xff00 && code <= 0xffef)
  )
}

class TestCanvasRenderingContext2D {
  font = ''
  measureText(text) {
    return { width: measureWidth(text, FONT_SIZE) }
  }
}

class TestOffscreenCanvas {
  constructor(_width, _height) {}
  getContext(_kind) {
    return new TestCanvasRenderingContext2D()
  }
}

Reflect.set(globalThis, 'OffscreenCanvas', TestOffscreenCanvas)

const { clearCache, prepareWithSegments, layoutWithLines } = await import(
  join(pretextRoot, 'src/layout.ts'),
)

clearCache()

const CASES = [
  {
    label: 'tap-count-ios-9',
    text: 'Platform: ios · Taps: 9',
    maxWidth: 294,
    lineHeight: LINE_HEIGHT,
  },
  {
    label: 'tap-count-ios-10',
    text: 'Platform: ios · Taps: 10',
    maxWidth: 294,
    lineHeight: LINE_HEIGHT,
  },
  {
    label: 'tap-count-ios-121',
    text: 'Platform: ios · Taps: 121',
    maxWidth: 294,
    lineHeight: LINE_HEIGHT,
  },
  {
    label: 'latin-punctuation-glue',
    text: 'Performance is critical for this kind of library.',
    maxWidth: 200,
    lineHeight: LINE_HEIGHT,
  },
  {
    label: 'cjk-wrap',
    text: '这是一段中文文本，用于测试文本布局库对中日韩字符的支持。每个字符之间都可以断行。',
    maxWidth: 160,
    lineHeight: LINE_HEIGHT,
  },
  {
    label: 'pre-wrap-tabs',
    text: 'Hello\tWorld',
    maxWidth: 120,
    lineHeight: LINE_HEIGHT,
    whiteSpace: 'pre-wrap',
  },
  {
    label: 'pre-wrap-newlines',
    text: 'Hello\nWorld',
    maxWidth: 200,
    lineHeight: LINE_HEIGHT,
    whiteSpace: 'pre-wrap',
  },
  {
    label: 'keep-all-cjk-latin',
    text: '日本語foo-bar',
    maxWidth: 140,
    lineHeight: LINE_HEIGHT,
    wordBreak: 'keep-all',
  },
  {
    label: 'latin-short',
    text: 'Alpha beta gamma',
    maxWidth: 80,
    lineHeight: LINE_HEIGHT,
  },
]

function layoutCase(testCase) {
  const options = {
    whiteSpace: testCase.whiteSpace ?? 'normal',
    wordBreak: testCase.wordBreak ?? 'normal',
    letterSpacing: testCase.letterSpacing ?? 0,
  }
  const prepared = prepareWithSegments(testCase.text, FONT, options)
  const layout = layoutWithLines(prepared, testCase.maxWidth, testCase.lineHeight)
  const lines = layout.lines.map((line) => line.text)
  const maxLineWidth = Math.max(0, ...layout.lines.map((line) => line.width))
  return {
    label: testCase.label,
    text: testCase.text,
    maxWidth: testCase.maxWidth,
    lineHeight: testCase.lineHeight,
    whiteSpace: options.whiteSpace,
    wordBreak: options.wordBreak,
    letterSpacing: options.letterSpacing,
    lines,
    lineCount: layout.lineCount,
    height: layout.height,
    maxLineWidth,
  }
}

const goldens = {
  version: 1,
  fontSize: FONT_SIZE,
  cases: [],
}

for (const testCase of CASES) {
  try {
    goldens.cases.push(layoutCase(testCase))
  } catch (error) {
    console.error(`Failed case ${testCase.label}:`, error)
    throw error
  }
}

const json = `${JSON.stringify(goldens, null, 2)}\n`
const hash = createHash('sha256').update(json).digest('hex')

const checkMode = process.argv.includes('--check')
if (checkMode) {
  if (!existsSync(outPath)) {
    console.error(`Missing golden fixture: ${outPath}`)
    console.error('Run: bun scripts/generate-text-layout-goldens.mjs')
    process.exit(1)
  }
  const existing = readFileSync(outPath, 'utf8')
  const existingHash = createHash('sha256').update(existing).digest('hex')
  if (existingHash !== hash) {
    console.error('text_layout_goldens.json is stale. Regenerate with:')
    console.error('  bun scripts/generate-text-layout-goldens.mjs')
    process.exit(1)
  }
  console.log('text_layout_goldens.json is up to date')
  process.exit(0)
}

writeFileSync(outPath, json)
console.log(`Wrote ${outPath} (${goldens.cases.length} cases, sha256=${hash.slice(0, 12)}…)`)
