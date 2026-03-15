import fs from 'fs';
import path from 'path';
import matter from 'gray-matter';

const docsDirectory = path.join(process.cwd(), 'docs');

export function getDocsSlugs() {
  if (!fs.existsSync(docsDirectory)) return [];
  const fileNames = fs.readdirSync(docsDirectory);
  return fileNames.filter(name => name.endsWith('.md') || name.endsWith('.mdx')).map(name => name.replace(/\.mdx?$/, ''));
}

export function getDocBySlug(slug: string) {
  if (!fs.existsSync(docsDirectory)) return null;
  const fullPathMDX = path.join(docsDirectory, `${slug}.mdx`);
  const fullPathMD = path.join(docsDirectory, `${slug}.md`);

  const fileContents = fs.existsSync(fullPathMDX) 
    ? fs.readFileSync(fullPathMDX, 'utf8') 
    : fs.existsSync(fullPathMD) ? fs.readFileSync(fullPathMD, 'utf8') : null;

  if (!fileContents) return null;

  const { data, content } = matter(fileContents);
  return {
    slug,
    meta: data,
    content,
  };
}

export function getAllDocs() {
  const slugs = getDocsSlugs();
  const docs = slugs
    .map((slug) => getDocBySlug(slug))
    .filter(doc => doc !== null);
  return docs;
}
