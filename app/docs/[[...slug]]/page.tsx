import { getDocBySlug, getDocsSlugs, getAllDocs } from '@/lib/mdx';
import { MDXRemote } from 'next-mdx-remote/rsc';
import rehypeHighlight from 'rehype-highlight';
import 'highlight.js/styles/atom-one-dark.css';
import Link from 'next/link';

export async function generateStaticParams() {
  const slugs = getDocsSlugs();
  return slugs.map((slug) => ({
    slug: slug === 'index' ? [] : [slug],
  }));
}

export default async function DocsPage({ params }: { params: { slug?: string[] } }) {
  const slugStr = params.slug ? params.slug.join('/') : 'index';
  const doc = getDocBySlug(slugStr);
  const allDocs = getAllDocs();

  if (!doc) {
    return (
      <main className="max-w-7xl mx-auto px-4 py-24 min-h-screen text-center">
        <h1 className="text-4xl font-bold text-white mb-4">Document Not Found</h1>
        <p className="text-gray-400">The requested MDX file does not exist.</p>
        <Link href="/docs" className="text-[#ccff00] mt-4 inline-block hover:underline">Return to Docs Home</Link>
      </main>
    );
  }

  return (
    <main className="max-w-7xl mx-auto px-4 pb-24 min-h-screen flex flex-col md:flex-row gap-8">
      {/* SIDEBAR */}
      <aside className="w-full md:w-64 shrink-0 border-r border-white/10 pt-24 pr-4">
        <h3 className="text-sm font-bold text-gray-500 uppercase tracking-widest mb-6 border-b border-white/10 pb-2">
          Documentation
        </h3>
        <nav className="flex flex-col gap-3 font-mono text-sm">
          {allDocs.map((d) => (
            <Link 
              key={d.slug} 
              href={`/docs${d.slug === 'index' ? '' : `/${d.slug}`}`}
              className={`transition-colors ${slugStr === d.slug ? 'text-[#ccff00] font-bold' : 'text-gray-400 hover:text-white'}`}
            >
              # {d.meta.title || d.slug}
            </Link>
          ))}
        </nav>
      </aside>

      {/* CONTENT */}
      <article className="flex-grow pt-24 prose prose-invert prose-lg max-w-none 
        prose-headings:font-sans prose-headings:font-bold prose-headings:tracking-tight 
        prose-h1:text-5xl prose-h1:text-white prose-h2:text-3xl prose-h2:text-[#ccff00] prose-h2:border-b prose-h2:border-white/10 prose-h2:pb-2 
        prose-a:text-[#00D1FF] prose-a:no-underline hover:prose-a:underline 
        prose-pre:bg-[#0a0a0a] prose-pre:border prose-pre:border-white/10 prose-pre:rounded-xl 
        prose-code:text-[#ccff00] prose-code:font-mono prose-code:bg-white/5 prose-code:px-1.5 prose-code:py-0.5 prose-code:rounded">
        
        <p className="text-gray-500 font-mono text-sm mb-8 -mt-4 border-b border-white/10 pb-4">
          Last Updated: {doc.meta.date || 'Unknown'} | Topic: {doc.meta.category || 'General'}
        </p>

        <MDXRemote 
          source={doc.content} 
          options={{
            mdxOptions: {
              rehypePlugins: [rehypeHighlight as any],
            }
          }} 
        />
      </article>
    </main>
  );
}
