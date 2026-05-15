import path from 'node:path';
import { promises as fs } from 'node:fs';

async function exists(filePath) {
  try {
    await fs.access(filePath);
    return true;
  } catch {
    return false;
  }
}

async function listDirectories(root) {
  try {
    const entries = await fs.readdir(root, { withFileTypes: true });
    return entries.filter((entry) => entry.isDirectory()).map((entry) => entry.name).sort();
  } catch {
    return [];
  }
}

async function listFiles(root, predicate = () => true) {
  const result = [];
  async function walk(current) {
    let entries = [];
    try {
      entries = await fs.readdir(current, { withFileTypes: true });
    } catch {
      return;
    }
    for (const entry of entries) {
      const fullPath = path.join(current, entry.name);
      if (entry.isDirectory()) {
        await walk(fullPath);
      } else if (predicate(fullPath)) {
        result.push(fullPath);
      }
    }
  }
  await walk(root);
  return result.sort();
}

function parseFunctions(headerText) {
  const functionPattern = /^\s*(?!typedef\b|#)(?:[A-Za-z_][A-Za-z0-9_]*\s+)*(?:const\s+)?(?:[A-Za-z_][A-Za-z0-9_]*|\*)[\s*]+([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{}]*\)\s*;/gm;
  const names = [];
  let match = functionPattern.exec(headerText);
  while (match) {
    names.push(match[1]);
    match = functionPattern.exec(headerText);
  }
  return names;
}

function parseFunctionPrototypes(headerText) {
  const functionPattern = /^\s*((?!typedef\b|#)(?:[A-Za-z_][A-Za-z0-9_]*\s+)*(?:const\s+)?(?:[A-Za-z_][A-Za-z0-9_]*|\*)[\s*]+([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{}]*\)\s*;)/gm;
  const prototypes = [];
  let match = functionPattern.exec(headerText);
  while (match) {
    prototypes.push(match[1].replace(/\s+/g, ' ').trim());
    match = functionPattern.exec(headerText);
  }
  return prototypes;
}

function parseTypeDeclarations(headerText) {
  const declarations = [];
  const enumPattern = /typedef\s+enum\s*(?:[A-Za-z_][A-Za-z0-9_]*)?\s*\{[\s\S]*?\}\s*[A-Za-z_][A-Za-z0-9_]*\s*;/g;
  const structPattern = /typedef\s+struct\s*(?:[A-Za-z_][A-Za-z0-9_]*)?\s*\{[\s\S]*?\}\s*[A-Za-z_][A-Za-z0-9_]*\s*;/g;
  const definePattern = /^\s*#define\s+(DRIVER_[A-Z0-9_]+|SERVICE_[A-Z0-9_]+)\b.*$/gm;

  for (const pattern of [enumPattern, structPattern, definePattern]) {
    let match = pattern.exec(headerText);
    while (match) {
      declarations.push(match[0].replace(/\s+\n/g, '\n').trim());
      match = pattern.exec(headerText);
    }
  }
  return declarations;
}

function parseBoardDefines(boardText) {
  const defines = {};
  const definePattern = /^\s*#define\s+(BOARD_[A-Z0-9_]+)\s+(.+?)\s*(?:\/\*.*)?$/gm;
  let match = definePattern.exec(boardText);
  while (match) {
    defines[match[1]] = match[2].trim();
    match = definePattern.exec(boardText);
  }
  return defines;
}

async function scanComponent(root, name) {
  const includeRoot = path.join(root, name, 'include');
  const headers = await listFiles(includeRoot, (filePath) => filePath.endsWith('.h'));
  const api = [];
  for (const header of headers) {
    const text = await fs.readFile(header, 'utf8');
    api.push({
      header: path.relative(root, header).replaceAll('\\', '/'),
      functions: parseFunctions(text),
      prototypes: parseFunctionPrototypes(text),
      declarations: parseTypeDeclarations(text)
    });
  }
  return { name, api };
}

export async function scanSdk(sdkRoot) {
  const componentsRoot = path.join(sdkRoot, 'components');
  const bspRoot = path.join(sdkRoot, 'bsp');
  const boardsRoot = path.join(sdkRoot, 'boards');
  const examplesRoot = path.join(sdkRoot, 'examples');
  const projectRoot = path.join(sdkRoot, 'project');

  const componentNames = await listDirectories(componentsRoot);
  const components = [];
  for (const component of componentNames) {
    components.push(await scanComponent(componentsRoot, component));
  }

  const boards = [];
  for (const board of await listDirectories(boardsRoot)) {
    const boardHeader = path.join(boardsRoot, board, 'board.h');
    const defaults = path.join(boardsRoot, board, 'sdkconfig.defaults');
    boards.push({
      name: board,
      hasBoardHeader: await exists(boardHeader),
      hasSdkconfigDefaults: await exists(defaults),
      defines: (await exists(boardHeader)) ? parseBoardDefines(await fs.readFile(boardHeader, 'utf8')) : {}
    });
  }

  const prompt0 = path.join(sdkRoot, 'prompt', 'prompt0.md');
  const sdkDevelop = path.join(sdkRoot, 'SDK_prompt', 'sdk_develop.md');

  return {
    scannedAt: new Date().toISOString(),
    sdkRoot,
    components,
    bsp: await listDirectories(bspRoot),
    boards,
    examples: await listDirectories(examplesRoot),
    projects: await listDirectories(projectRoot),
    docs: (await listFiles(path.join(sdkRoot, 'docs'))).map((filePath) => path.relative(sdkRoot, filePath).replaceAll('\\', '/')),
    prompts: {
      hasPrompt0: await exists(prompt0),
      prompt0: (await exists(prompt0)) ? await fs.readFile(prompt0, 'utf8') : '',
      hasSdkDevelop: await exists(sdkDevelop),
      sdkDevelop: (await exists(sdkDevelop)) ? await fs.readFile(sdkDevelop, 'utf8') : ''
    }
  };
}
