// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
//
// SPDX-License-Identifier: Apache-2.0

// What a framework's glue gets from generate() and what it gives back.
//
// A framework's app_<fw>.cpp is not a static file: it is built from the slots
// every composed block contributes, from the other frameworks beside it, and
// from what the catalog offers. Each framework module in this directory owns
// that code and the files it writes.

import type { Product, RenderedInstance } from '../types.js'
import type { CatalogFeatures } from '../generator-common.js'
import type { BoardFacts } from '../generator.js'

export interface FrameworkContext {
  /** Every rendered instance: the product's blocks, then its frameworks. */
  rendered: RenderedInstance[]
  features: CatalogFeatures
  /** True when the product composes at least one device type. */
  hasDeviceTypes: boolean
  /** The product with its defaults applied. */
  product: Product
  /** The product's own params, as given (`params:`). */
  productParams: Record<string, string>
  /** Instances the board contributes (its display), empty without a board. */
  boardExtras: RenderedInstance[]
  bmgrBoard: string | null | undefined
  boardFacts: BoardFacts | null
}

/** A file to write, relative to the generated tree's root. */
export interface GeneratedFile {
  path: string
  content: string
}

export interface FrameworkGlue {
  /** The framework id: its block is `frameworks/<id>`. */
  id: string
  /** The files to write when the product selects this framework, in order. */
  files(ctx: FrameworkContext): GeneratedFile[]
}
