// SPDX-License-Identifier: LGPL-3.0-or-later
.pragma library

function parse(serialized) {
    let names
    try { names = JSON.parse(serialized) }
    catch (error) { return {} }
    if (!names || typeof names !== "object" || Array.isArray(names)) return {}
    const result = {}
    for (const id of Object.keys(names)) {
        if (typeof names[id] === "string" && names[id].trim().length > 0) result[id] = names[id]
    }
    return result
}
