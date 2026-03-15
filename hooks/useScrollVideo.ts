import { useRef, useState, useEffect } from 'react';
import { useScroll, useMotionValueEvent, useTransform } from 'framer-motion';

/**
 * Hook to scrub a video element based on the scroll position.
 * @param containerRef Reference to the scrollable container.
 * @returns An object with the video reference to attach to the <video> element.
 */
export function useScrollVideo(containerRef: React.RefObject<HTMLElement>) {
  const videoRef = useRef<HTMLVideoElement>(null);
  const [duration, setDuration] = useState<number>(0);

  // Measure the total scroll progress from the container (sticky section)
  const { scrollYProgress } = useScroll({
    target: containerRef,
    offset: ['start start', 'end end'],
  });

  // Transform scroll progress (0..1) to video time (0..duration)
  const videoTime = useTransform(scrollYProgress, [0, 1], [0, duration || 1]);

  useEffect(() => {
    const video = videoRef.current;
    if (!video) return;

    const handleLoadedMetadata = () => {
      setDuration(video.duration);
    };

    video.addEventListener('loadedmetadata', handleLoadedMetadata);
    // If cache kicks in, the duration might already be there
    if (video.readyState >= 1) {
      handleLoadedMetadata();
    }

    return () => {
      video.removeEventListener('loadedmetadata', handleLoadedMetadata);
    };
  }, []);

  // Update video currentTime on scroll changes
  useMotionValueEvent(videoTime, 'change', (latestTime) => {
    if (videoRef.current && duration > 0) {
      // Use requestAnimationFrame for smoother updates (often required on Safari/Chrome)
      requestAnimationFrame(() => {
        if (videoRef.current) {
          videoRef.current.currentTime = latestTime;
        }
      });
    }
  });

  return { videoRef, scrollYProgress };
}
