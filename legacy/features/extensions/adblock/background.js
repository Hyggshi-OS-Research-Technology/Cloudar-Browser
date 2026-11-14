(function() {
    'use strict';

    // ─── AdBlock content script ────────────────────────────────────────────
    // Works alongside the network-level blocker (features/adblock.py, which
    // blocks requests to known ad/tracker domains before they leave the
    // process). This script handles what network blocking alone can't:
    //   - Cosmetic hiding: some ad containers stay in the DOM (empty) even
    //     once their network request is blocked, leaving an empty gap.
    //   - YouTube specifically: skip-button auto-click, muting non-skippable
    //     ads, and hiding masthead/display/companion ads — since some
    //     YouTube ads are stitched into the video stream itself (SSAI) and
    //     simply cannot be blocked at the network level.

    if (window.location.protocol === 'cloudar:' || window.location.protocol === 'about:') {
        return;
    }

    // ─── Generic cosmetic hiding (all sites) ───────────────────────────────
    const GENERIC_HIDE_SELECTORS = [
        'ins.adsbygoogle',
        'iframe[id^="google_ads_iframe"]',
        'div[id^="google_ads_iframe"]',
        'div[id^="div-gpt-ad"]',
        'div[class*="gpt-ad"]',
        '[data-ad-slot]',
        '[data-ad-client]',
        '.ad-container:not(body):not(html)',
        '.advertisement:not(body):not(html)',
        '.adsbygoogle-noablate',
        'div[aria-label="Advertisement"]',
        'div[id*="taboola"]',
        'div[id*="outbrain"]',
        'iframe[src*="doubleclick.net"]',
        'iframe[src*="googlesyndication.com"]',
    ];

    function injectStyle(id, css) {
        if (document.getElementById(id)) return;
        const style = document.createElement('style');
        style.id = id;
        style.textContent = css;
        (document.head || document.documentElement).appendChild(style);
    }

    injectStyle('cloudar-adblock-generic-style',
        GENERIC_HIDE_SELECTORS.map(s => s + ' { display: none !important; visibility: hidden !important; }').join('\n')
    );

    // Defense-in-depth: some ad scripts re-insert inline `style="display:block"`
    // after our stylesheet applies, or add the ad container dynamically.
    // A lightweight observer catches those without scanning the whole page
    // on every mutation.
    function hideIfAd(node) {
        if (!(node instanceof Element)) return;
        for (const sel of GENERIC_HIDE_SELECTORS) {
            try {
                if (node.matches && node.matches(sel)) {
                    node.style.setProperty('display', 'none', 'important');
                }
                node.querySelectorAll && node.querySelectorAll(sel).forEach(el => {
                    el.style.setProperty('display', 'none', 'important');
                });
            } catch (e) { /* invalid selector on this DOM - ignore */ }
        }
    }

    const genericObserver = new MutationObserver(function(mutations) {
        for (const m of mutations) {
            for (const node of m.addedNodes) hideIfAd(node);
        }
    });
    genericObserver.observe(document.documentElement, { childList: true, subtree: true });

    // ─── YouTube-specific handling ──────────────────────────────────────────
    function isYouTubeSite() {
        const host = window.location.hostname;
        return host === 'youtu.be' || host === 'youtube.com' || host.endsWith('.youtube.com');
    }

    if (!isYouTubeSite()) return;

    // Hide masthead/display/companion/in-feed ads. These are separate DOM
    // elements YouTube renders around the page (not the video ad itself),
    // so hiding them is safe and has no effect on playback.
    injectStyle('cloudar-adblock-youtube-style', `
        #masthead-ad,
        ytd-display-ad-renderer,
        ytd-promoted-sparkles-web-renderer,
        ytd-promoted-video-renderer,
        ytd-companion-slot-renderer,
        ytd-action-companion-ad-renderer,
        ytd-in-feed-ad-layout-renderer,
        ytd-ad-slot-renderer,
        ytd-banner-promo-renderer,
        ytd-statement-banner-renderer,
        ytd-primetime-promo-renderer,
        .ytd-video-masthead-ad-v3-renderer,
        #player-ads,
        .ytp-ad-overlay-container,
        .ytp-ad-text-overlay,
        .ytp-ad-image-overlay {
            display: none !important;
        }
    `);

    // Auto-click the Skip Ad button as soon as it becomes clickable.
    const SKIP_SELECTORS = [
        '.ytp-ad-skip-button',
        '.ytp-ad-skip-button-modern',
        '.ytp-skip-ad-button',
        'button.ytp-ad-skip-button-slot',
    ];

    function tryClickSkip() {
        for (const sel of SKIP_SELECTORS) {
            const btn = document.querySelector(sel);
            if (btn) {
                btn.click();
                return true;
            }
        }
        return false;
    }

    // Mute the player while a non-skippable ad is playing, and restore the
    // user's previous mute/volume state once it ends. We only ever touch
    // mute state while `.ad-showing`/`.ad-interrupting` is present on the
    // player, so normal video playback is never affected.
    let savedMuted = null;
    let adCurrentlyShowing = false;

    function getPlayer() {
        return document.querySelector('#movie_player, .html5-video-player');
    }

    function getVideoEl() {
        return document.querySelector('video.html5-main-video, video');
    }

    function isAdShowing(player) {
        if (!player) return false;
        return player.classList.contains('ad-showing') || player.classList.contains('ad-interrupting');
    }

    function handleAdState() {
        const player = getPlayer();
        const showing = isAdShowing(player);
        const video = getVideoEl();

        if (showing && !adCurrentlyShowing) {
            adCurrentlyShowing = true;
            if (video && savedMuted === null) {
                savedMuted = video.muted;
                video.muted = true;
            }
        } else if (!showing && adCurrentlyShowing) {
            adCurrentlyShowing = false;
            if (video && savedMuted !== null) {
                video.muted = savedMuted;
                savedMuted = null;
            }
        }

        if (showing) tryClickSkip();
    }

    // Poll at a modest interval — cheap, and far more reliable across
    // YouTube's frequent player DOM changes than trying to hook every
    // possible mutation/event source.
    setInterval(handleAdState, 400);

})();
