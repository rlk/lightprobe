#!/Users/rlk/Build/racket/bin/racket

;;;; LIGHTPROBE COMPOSER Copyright (C) 2010 Robert Kooima
;;;;
;;;; This program is free software: you can redistribute it and/or modify it
;;;; under the terms of the GNU General Public License as published by the Free
;;;; Software Foundation, either version 3 of the License, or (at your option)
;;;; any later version.
;;;;
;;;; This program is distributed in the hope that it will be useful, but
;;;; WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
;;;; or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
;;;; for more details.

(module lp-compose racket/gui
  
  ;;----------------------------------------------------------------------------

  (define default-path (string->path "Untitled"))

  (define lightprobe #f)
  (define lightprobe-path default-path)
  
  (define (set-lightprobe-path! path)
    (set! lightprobe-path path)
    (send root set-label (path->string path)))

  ;;----------------------------------------------------------------------------
  ;; Rendering and image processing are performed by the graphics hardware via
  ;; the OpenGL Shading Language. This is managed by the lightprobe library, a
  ;; dynamically-loaded native code module written in C. The following FFI
  ;; binds that library API.

  (require ffi/unsafe)

  (define lp-lib (ffi-lib "lp-render"))

  (define (lp-ffi str sig)
    (get-ffi-obj str lp-lib sig))

  ;;----------------------------------------------------------------------------
  ;; Those foreign functions that access the OpenGL context must hold the global
  ;; lock during execution. The following definition wraps such functions as
  ;; necessary. The context is defined when it becomes available.

  (define lp-context #f)

  (define (gl-ffi str sig)
    (let ((fun (lp-ffi str sig)))
      (lambda arg
        (if lp-context
            (send lp-context call-as-current (lambda () (apply fun arg)))
            #f))))

  ;;----------------------------------------------------------------------------
  ;; Lightprobe instantiation

  (define lp-init
    (gl-ffi "lp_init" (_fun          -> _pointer)))
  (define lp-free
    (gl-ffi "lp_free" (_fun _pointer -> _void)))

  ;;----------------------------------------------------------------------------
  ;; Image instantiation

  (define lp-add-image
    (gl-ffi "lp_add_image" (_fun _pointer _path -> _int)))
  (define lp-del-image
    (gl-ffi "lp_del_image" (_fun _pointer _int  -> _void)))

  (define lp-get-image-width
    (lp-ffi "lp_get_image_width"  (_fun _pointer _int -> _int)))
  (define lp-get-image-height
    (lp-ffi "lp_get_image_height" (_fun _pointer _int -> _int)))

  ;;----------------------------------------------------------------------------
  ;; Raw image flag accessors

  (define lp-set-image-flags
    (lp-ffi "lp_set_image_flags" (_fun _pointer _int _int -> _void)))
  (define lp-clr-image-flags
    (lp-ffi "lp_clr_image_flags" (_fun _pointer _int _int -> _void)))
  (define lp-get-image-flags
    (lp-ffi "lp_get_image_flags" (_fun _pointer _int      -> _int)))

  ;;----------------------------------------------------------------------------
  ;; Raw image value accessors

  (define lp-set-image-value
    (lp-ffi "lp_set_image_value" (_fun _pointer _int _int _float -> _void)))
  (define lp-get-image-value
    (lp-ffi "lp_get_image_value" (_fun _pointer _int _int        -> _float)))

  ;;----------------------------------------------------------------------------
  ;; Rendering

  (define lp-render-grid 1)
  (define lp-render-res  2)

  (define lp-render-circle
    (gl-ffi "lp_render_circle" 
      (_fun _pointer _int _int _int _float _float _float _float -> _void)))
  (define lp-render-sphere
    (gl-ffi "lp_render_sphere" 
      (_fun _pointer _int _int _int _float _float _float _float -> _void)))

  ;;----------------------------------------------------------------------------
  ;; Export

  (define lp-export-cube
    (gl-ffi "lp_export_cube"   (_fun _pointer _path -> _bool)))
  (define lp-export-sphere
    (gl-ffi "lp_export_sphere" (_fun _pointer _path -> _bool)))

  ;;----------------------------------------------------------------------------
  ;; Image flag accessors

  (define (lp-is-image? d b)
    (and (not (negative? d))
         (not (zero? (bitwise-and b (lp-get-image-flags lightprobe d))))))

  (define (lp-is-image-loaded? d) (lp-is-image? d 1))
  (define (lp-is-image-active? d) (lp-is-image? d 2))
  (define (lp-is-image-hidden? d) (lp-is-image? d 4))

  (define (lp-is-image-visible? d) (not (lp-is-image-hidden? d)))

  ;;----------------------------------------------------------------------------
  ;; Image value accessors

  (define (lp-set-value d k v) (lp-set-image-value lightprobe d k v))
  (define (lp-get-value d k)   (lp-get-image-value lightprobe d k))

  (define (lp-set-circle-x         d v) (lp-set-value d 0 v))
  (define (lp-get-circle-x         d)   (lp-get-value d 0))
  (define (lp-set-circle-y         d v) (lp-set-value d 1 v))
  (define (lp-get-circle-y         d)   (lp-get-value d 1))
  (define (lp-set-circle-radius    d v) (lp-set-value d 2 v))
  (define (lp-get-circle-radius    d)   (lp-get-value d 2))
  (define (lp-set-sphere-elevation d v) (lp-set-value d 3 v))
  (define (lp-get-sphere-elevation d)   (lp-get-value d 3))
  (define (lp-set-sphere-azimuth   d v) (lp-set-value d 4 v))
  (define (lp-get-sphere-azimuth   d)   (lp-get-value d 4))
  (define (lp-set-sphere-roll      d v) (lp-set-value d 5 v))
  (define (lp-get-sphere-roll      d)   (lp-get-value d 5))

  ;;----------------------------------------------------------------------------

  (define (round->exact x) (inexact->exact (round x)))

  ;;----------------------------------------------------------------------------
  ;; The Apple HIG defines a preferences panel with all radio and check boxes
  ;; vertically aligned, with a top-right-justified label to the left of each
  ;; group. The preferences-group is a custom layout pane to produce this.
  
  (define preferences-group%
    (class pane%
      (init-field fraction)
      
      (define/override (container-size info)
        (let ((lw (caar   info))
              (lh (cadar  info))
              (rw (caadr  info))
              (rh (cadadr info)))

          (let ((ll (inexact->exact (ceiling (/ lw      fraction))))
                (rr (inexact->exact (ceiling (/ rw (- 1 fraction))))))

            (values (max ll rr) (max lh rh)))))
      
      (define/override (place-children info width height)
        (let ((lw (caar   info))
              (lh (cadar  info))
              (rw (caadr  info))
              (rh (cadadr info))

              (x  (inexact->exact (round (* fraction width)))))
          
            (list (list (- x lw) 0 lw height)
                  (list    x     0 rw height))))
      
      (super-new [stretchable-height #f])))

  ;;----------------------------------------------------------------------------
  ;; simple-radio-box is a radio box with vertical layout and no label. This is
  ;; useful for building preference groups.

  (define simple-radio-box%
    (class radio-box%
      (super-new [label #f] [style '(vertical)])))

  ;;----------------------------------------------------------------------------
  ;; packed-button is a button with no margin. This allows buttons to pack a
  ;; little tighter and look better under OSX.

  (define packed-button%
    (class button%
      (super-new [vert-margin 0] [horiz-margin 0] [stretchable-width #t])))

  ;;----------------------------------------------------------------------------
  ;; drop-list-box is a list-box that accepts drag-and-drop events, allowing a
  ;; callback function to handle them.  TODO: does not work with v5.0.99.4.

  (define drop-list-box%
    (class list-box%
      (init-field drop-callback)

      (define/override (on-drop-file path)
                       (drop-callback path))

      (super-new)
      (send this accept-drop-files #t)))

  ;;----------------------------------------------------------------------------
  ;; drop-frame is a frame that accepts drag-and-drop events, allowing a call-
  ;; back function to handle them.

  (define drop-frame%
    (class frame%
      (init-field drop-callback)

      (define/override (on-drop-file path)
                       (drop-callback path))

      (super-new)
      (send this accept-drop-files #t)))

  ;;----------------------------------------------------------------------------
  ;; The lightprobe viewer has two modes: Image view and Environment view.
  ;; The view-mode preference group manages the current mode state.

  (define view-mode%
    (class preferences-group%
      (super-new [fraction 0.4])
      (init-field notify)

      ; GUI sub-elements
      
      (define label (new message%          [parent this]
                                           [label "Mode:"]))
      (define radio (new simple-radio-box% [parent this]
                                           [callback (lambda x (notify))]
                                           [choices '("Image" "Environment")]))
      ; Public interface

      (define/public (set-mode n) (send radio set-selection n))
      (define/public (get-mode)   (send radio get-selection))))

  ;;----------------------------------------------------------------------------
  ;; The lightprobe viewer has a number of rendering options. The view-options
  ;; preference group manages these.

  (define view-options%
    (class preferences-group%
      (super-new [fraction 0.4])
      (init-field notify)
      
      ; GUI sub-elements
      
      (define label (new message%       [parent this]
                                        [label "View:"]))
      (define group (new vertical-pane% [parent this]
                                        [alignment '(left top)]))
      (define grid  (new check-box%     [parent group]
                                        [label "Show Grid"]
                                        [callback (lambda x (notify))]))
      (define res   (new check-box%     [parent group]
                                        [label "Show Resolution"]
                                        [callback (lambda x (notify))]))

      ; Public interface

      (define/public (get-grid) (send grid get-value))
      (define/public (get-res)  (send res  get-value))))

  ;;----------------------------------------------------------------------------
  ;; The lightprobe viewer has a few variables that tune the rendering. The
  ;; view-values group manages these, with the lightprobe canvas as observer.

  (define view-values%
    (class horizontal-pane%
      (super-new)
      (init-field notify)

      (define width    300)
      (define min-zoom -20)
      (define max-zoom  50)

      ; GUI sub-elements

      (define fill (new pane%   [parent this]))
      (define expo (new slider% [parent this]
                                [label "Exposure"]
                                [init-value  100]
                                [min-value     0]
                                [max-value width]
                                [min-width width]
                                [stretchable-width #f]
                                [style '(horizontal plain)]
                                [callback (lambda x (notify))]))
      (define zoom (new slider% [parent this]
                                [label "Zoom"]
                                [init-value  0]
                                [min-value min-zoom]
                                [max-value max-zoom]
                                [min-width width]
                                [stretchable-width #f]
                                [style '(horizontal plain)]
                                [callback (lambda x (notify))]))

      (define (value->expo v) (* 50.0 (/ v width)))
      (define (value->zoom v) (exp (/ v 10.0)))
      (define (zoom->value z) (* (log z) 10.0))

      ; Public interface

      (define/public (get-expo)
        (exact->inexact (value->expo (send expo get-value))))

      (define/public (get-zoom)
        (exact->inexact (value->zoom (send zoom get-value))))

      (define/public (set-zoom z)
        (let ((v (round->exact (zoom->value z))))
          (send zoom set-value (max (min v max-zoom) min-zoom))))))

  ;;----------------------------------------------------------------------------
  ;; The image-list manages the set of lightprobe images input to the composer.
  ;; It allows the addition and removal of images, hiding and showing of loaded
  ;; images, and the selection of a subset of images for configuration.

  (define image-list%
    (class horizontal-pane%
      (super-new)
      (init-field notify)

      ; The "index" is the image's GUI list-box position.

      (define (get-indices)
        (build-list (send images get-number) values))

      (define (get-index s)
        (send images find-string s))
      
      ; The "descr" is the image's lightprobe API descriptor.
    
      (define (index->descr i) (send images get-data i))

      (define (get-descrs) (map index->descr (get-indices)))
      (define (get-descr s)    (index->descr (get-index s)))

      ; Interaction callbacks

      (define (do-add  control event)
        (map (lambda (p)
               (send this add-image p))
             (or (get-file-list) '()))
        (notify))

      (define (do-del  control event)
        (map (lambda (i)
               (send this del-image i))
             (send images get-selections))
        (notify))

      (define (do-hide control event)
        (map (lambda (i)
               (lp-set-image-flags lightprobe (index->descr i) 4))
             (send images get-selections))
        (notify))

      (define (do-show control event)
        (map (lambda (i)
               (lp-clr-image-flags lightprobe (index->descr i) 4))
             (send images get-selections))
        (notify))

      ; Set the 'active' flag on all images based on selection status.

      (define (set control event)
        (map (lambda (i)
               (let ((d (index->descr i)))
                 (if (send images is-selected? i)
                     (lp-set-image-flags lightprobe d 2)
                     (lp-clr-image-flags lightprobe d 2))))
             (get-indices)))

      ; GUI sub-elements

      (define buttons (new vertical-pane% [parent this]
                                          [stretchable-width #f]))
      (define images  (new list-box%      [parent this]
                                          [label #f]
                                          [choices '()]
                                          [callback set]
                                          [style '(extended)]))

      (define add  (instantiate packed-button% ("Add"    buttons do-add)))
      (define rem  (instantiate packed-button% ("Remove" buttons do-del)))
      (define hide (instantiate packed-button% ("Hide"   buttons do-hide)))
      (define show (instantiate packed-button% ("Show"   buttons do-show)))

      ; Add (load) the named image.

      (define/public (add-image path)
        (let ((d (lp-add-image lightprobe path)))

          (if (lp-is-image-loaded? d)
              (begin (send images append (path->string path) d)
                     (notify)
                     #t)
              #f)))

      ; Remove (unload) the named image.

      (define/public (del-image i)
        (let ((d (index->descr i)))
          (begin (lp-del-image lightprobe d)
                 (send images delete      i))
          (notify)))

      ; Write the current image state to the named file.

      (define/public (save-file path)
        (let ((write-image
               (lambda (i)
                 (let* ((d (send images get-data   i))
                        (s (send images get-string i)))

                   (printf "~s ~s ~s ~s ~s ~s ~s ~s~n"
                           (lp-get-image-flags lightprobe d)
                           (lp-get-circle-x         d)
                           (lp-get-circle-y         d)
                           (lp-get-circle-radius    d)
                           (lp-get-sphere-elevation d)
                           (lp-get-sphere-azimuth   d)
                           (lp-get-sphere-roll      d) s)))))

          (with-output-to-file path
            (lambda () (map write-image (get-indices)))
            #:mode 'text #:exists 'replace))

        (send root set-label (path->string path)))

      ; Load the named file to the current image state.

      (define/public (load-file path)
        (let ((parse-image (lambda (line)
                             (let* ((in (open-input-string line))
                                    (f  (read in))
                                    (cx (read in))
                                    (cy (read in))
                                    (cr (read in))
                                    (se (read in))
                                    (sa (read in))
                                    (sr (read in))
                                    (nm (read in)))
        
                              (add-image (string->path nm))))))

          (let loop ((in (open-input-file path)))
            (let ((line (read-line in)))
              (if (string? line)
                  (begin
                    (parse-image line)
                    (loop in))
                  (void)))))

        (send root set-label (path->string path)))

      ; Unload all currently-loaded images.

      (define/public (init-file)
        (map (lambda (i)
               (send this del-image i)) (get-indices))
        (notify))

      ; Return a list of path strings of all currently-loaded images.

      (define/public (get-visible-descrs)
        (filter lp-is-image-visible? (get-descrs)))))

  ;;----------------------------------------------------------------------------

  (define lp-menu-bar%
    (class menu-bar%
      (super-new)
      (init-field save-file)
      (init-field load-file)
      (init-field init-file)

      ; File / New

      (define (do-new control event)
        (init-file)
        (set-lightprobe-path! default-path))

      ; File Save

      (define (do-save control event)
        (save-file lightprobe-path))

      ; File / Open

      (define (do-open control event)
        (let ((path (get-file)))
          (if path
              (begin
                (init-file)
                (set-lightprobe-path! path)
                (load-file lightprobe-path))
              (void))))

      ; File / Save As...

      (define (do-save-as control event)
        (let ((path (put-file)))
          (if path
              (begin
                (set-lightprobe-path! path)
                (save-file lightprobe-path))
              (void))))

      ; File / Export Cube...

      (define (do-export-cube control event)
        (let ((path (put-file)))
          (if path (lp-export-cube   lightprobe path) #f)))

      ; File / Export Sphere...

      (define (do-export-sphere control event)
        (let ((path (put-file)))
          (if path (lp-export-sphere lightprobe path) #f)))

      ; Menus and menu items.

      (define (get-shifted-shortcut-prefix)
        (cons 'shift (get-default-shortcut-prefix)))

      (let ((file (new menu% [parent this] [label "File"])))

        (new menu-item% [parent file]
                        [label "New"]
                        [callback do-new]
                        [shortcut #\n])
        (new menu-item% [parent file]
                        [label "Open"]
                        [callback do-open]
                        [shortcut #\o])

        (new separator-menu-item% [parent file])

        (new menu-item% [parent file]
                        [label "Save"]
                        [callback do-save]
                        [shortcut #\s])
        (new menu-item% [parent file]
                        [label "Save As..."]
                        [callback do-save-as]
                        [shortcut #\s]
                        [shortcut-prefix (get-shifted-shortcut-prefix)])

        (new separator-menu-item% [parent file])

        (new menu-item% [parent file]
                        [label "Export Cube Map..."]
                        [callback do-export-cube]
                        [shortcut #\e])
        (new menu-item% [parent file]
                        [label "Export Sphere Map..."]
                        [callback do-export-sphere]
                        [shortcut #\e]
                        [shortcut-prefix (get-shifted-shortcut-prefix)]))))

  ;;----------------------------------------------------------------------------

  ;; The lp-canvas handles all lightprobe rendering. As creator of the OpenGL
  ;; context, it directs the startup and initialization of all lightprobe
  ;; resources. It receives a number of thunks for use in marshalling rendering
  ;; parameters, and implements all mouse and scroll bar interaction.

  (define lp-canvas%
    (class canvas%
      (inherit refresh with-gl-context swap-gl-buffers)

      (init-field load-file)
      (init-field set-zoom)
      (init-field get-zoom)
      (init-field get-expo)
      (init-field get-mode)
      (init-field get-images)
      (init-field get-grid)
      (init-field get-res)

      (super-new [style '(gl hscroll vscroll no-autoclear)])

      (send this init-manual-scrollbars 1000 1000 1000 1000 0 0)

      ;; Compute the normalized scroll position: position / range.

      (define (get-x)
        (let ((x (send this get-scroll-pos   'horizontal))
              (w (send this get-scroll-range 'horizontal)))
          (if (positive? w)
              (exact->inexact (/ x w))
              0.0)))

      (define (get-y)
        (let ((y (send this get-scroll-pos   'vertical))
              (h (send this get-scroll-range 'vertical)))
          (if (positive? h)
              (exact->inexact (/ y h))
              0.0)))

      ;; The virtual size of the canvas contents is defined to be the size
      ;; of the largest currently-visible image times the zoom factor, or
      ;; one if there are no visible images.

      (define (get-contents-w)
        (let* ((ws (map (lambda (d)
                          (lp-get-image-width lightprobe d)) (get-images))))
          (if (null? ws)
              1
              (round->exact (* (get-zoom) (apply max ws))))))

      (define (get-contents-h)
        (let* ((hs (map (lambda (d)
                          (lp-get-image-height lightprobe d)) (get-images))))
          (if (null? hs)
              1
              (round->exact (* (get-zoom) (apply max hs))))))

      ;; The center of the image should not move during zooming.  Thus, the
      ;; pan must be adjusted as image and zoom parameters change.  To make
      ;; this possible, we track the current image center in normalized image
      ;; coordinates, updating it with each reshape and scroll.

      (define center-x 0.5)
      (define center-y 0.5)

      (define (recenter)
        (let* ((iw (get-contents-w))
               (ih (get-contents-h))
               (hw (/ (send this get-width)  2))
               (hh (/ (send this get-height) 2))
               (sx (send this get-scroll-pos 'horizontal))
               (sy (send this get-scroll-pos 'vertical)))

          (set! center-x (/ (+ sx hw) iw))
          (set! center-y (/ (+ sy hh) ih))))

      ; The reshape function is responsible for handling changes to the shape
      ; of the canvas.  Beyond changes to the shape of the parent window, this
      ; may occur in response to additions, deletions, and visibility changes
      ; among the current set of loaded images.  The primary task is to ensure
      ; that the scroll bars make sense given the content currently on display.
      ; With this done, cache the image center and trigger a re-paint.

      (define/public (reshape)
        (let* ((iw (get-contents-w))
               (ih (get-contents-h))

               (ww (send this get-width))
               (wh (send this get-height))

               (nw (max 0 (- iw ww)))
               (nh (max 0 (- ih wh)))
               (nx (round->exact (max 0 (- (* center-x iw) (/ ww 2)))))
               (ny (round->exact (max 0 (- (* center-y ih) (/ wh 2))))))

          (send this set-scroll-page  'horizontal ww)
          (send this set-scroll-range 'horizontal nw)
          (send this set-scroll-pos   'horizontal nx)
          (send this set-scroll-page  'vertical   wh)
          (send this set-scroll-range 'vertical   nh)
          (send this set-scroll-pos   'vertical   ny)

          (recenter)
          (refresh)))

      ; Left mouse button events configure the current image, while right mouse
      ; events act as shortcuts to panning and zooming. 

      (define click-op      #f)
      (define click-x        0)
      (define click-y        0)
      (define click-z        0)
      (define click-scroll-x 0)
      (define click-scroll-y 0)

      (define/override (on-event event)
        (let ((shift? (send event get-shift-down))
              (cx     (send event get-x))
              (cy     (send event get-y))
              (cz     (get-zoom))
              (sx     (send this get-scroll-pos 'horizontal))
              (sy     (send this get-scroll-pos 'vertical)))

          (case (send event get-event-type)

            ((right-down)
             (set! click-x        cx)
             (set! click-y        cy)
             (set! click-z        cz)
             (set! click-scroll-x sx)
             (set! click-scroll-y sy)
             (set! click-op (if shift? 'zoom 'pan)))

            ((motion)
             (case click-op

               ;; Right-click shift-drag = zoom

               ((zoom)
                (set-zoom (max 0.1 (+ click-z (/ (- click-y cy) 50))))
                (reshape))

               ;; Right-click drag = pan

               ((pan)
                (let ((nx (max 0 (+ click-scroll-x (- click-x cx))))
                      (ny (max 0 (+ click-scroll-y (- click-y cy)))))
                  (send this set-scroll-pos 'horizontal nx)
                  (send this set-scroll-pos 'vertical   ny)
                  (recenter)
                  (reshape)))

               (else (void))))

            (else (set! click-op #f)))))

      ; Scroll events and canvas resizes must recache the image center and
      ; update the display.
      
      (define/override (on-scroll event) (recenter) (send this refresh))
      (define/override (on-size   w h)   (recenter) (send this reshape))

      ; The on-paint function redraws the canvas.  This involves marshalling
      ; all of the parameters maintained by other GUI elements and calling the
      ; proper render function for the current view mode.

      (define/override (on-paint)
        (let ((x (get-x))
              (y (get-y))
              (w (send this get-width))
              (h (send this get-height))
              (e (get-expo))
              (z (get-zoom))
              (f (bitwise-ior (if (get-grid) lp-render-grid 0)
                              (if (get-res)  lp-render-res  0))))

          (case (get-mode)
            ((0) (lp-render-circle lightprobe f w h x y e z))
            ((1) (lp-render-sphere lightprobe f w h x y e z)))

          (with-gl-context (lambda () (swap-gl-buffers)))))

      ; OpenGL use must wait until the canvas has been shown and the context
      ; created. Do all lightprobe and image state initialization here. Load
      ; any data file listed on the command line.

      (define/override (on-superwindow-show shown?)
        (if shown?
            (let ((ctx (send (send this get-dc) get-gl-context))
                  (arg (current-command-line-arguments)))

              (set! lp-context ctx)
              (set! lightprobe (lp-init))

              (if (and (vector? arg) (positive? (vector-length arg)))

                  (let ((path (string->path (vector-ref arg 0))))
                    (load-file            path)
                    (set-lightprobe-path! path))

                  (void))) (void)))))

  ;;----------------------------------------------------------------------------

  ;; The lp-frame is the toplevel window of the application, and the overall
  ;; organizing structure through which all state changes flow. All GUI sub-
  ;; groups are instantiated as members and a variety of anonymous functions
  ;; are closed within this scope to provide cross-cutting access between
  ;; sub-elements of the application.

  (define lp-frame%
    (class drop-frame%
      (super-new [label (path->string default-path)]
                 [drop-callback (lambda (path) (send images add-image path))])

      ; Layout panes

      (define top (new horizontal-pane% [parent this] [stretchable-height #f]))
      (define mid (new horizontal-pane% [parent this] [stretchable-height #t]))
      (define bot (new horizontal-pane% [parent this] [stretchable-height #f]))

      (define img (new group-box-panel% [parent top]
                                        [label "Images" ]
                                        [stretchable-width #t]))
      (define opt (new group-box-panel% [parent top]
                                        [label "Options"]
                                        [stretchable-width #f]))

      ; GUI elements

      (define menus
        (new lp-menu-bar%
             [parent this]
             [save-file  (lambda (path) (send images save-file path))]
             [load-file  (lambda (path) (send images load-file path))]
             [init-file  (lambda ()     (send images init-file))]))

      (define canvas
        (new lp-canvas%
             [parent mid]
             [min-width  640]
             [min-height 480]
             [load-file  (lambda (path) (send images load-file path))]
             [set-zoom   (lambda (z)    (send values  set-zoom z))]
             [get-zoom   (lambda ()     (send values  get-zoom))]
             [get-expo   (lambda ()     (send values  get-expo))]
             [get-grid   (lambda ()     (send options get-grid))]
             [get-mode   (lambda ()     (send mode    get-mode))]
             [get-res    (lambda ()     (send options get-res))]
             [get-images (lambda ()     (send images  get-visible-descrs))]))

      (define images
        (new image-list%
             [parent img]
             [notify (lambda () (send canvas reshape))]))

      (define mode
        (new view-mode%
             [parent opt]
             [notify (lambda () (send canvas reshape))]))

      (define options
        (new view-options%
             [parent opt]
             [notify (lambda () (send canvas reshape))]))

      (define values
        (new view-values%
             [parent bot]
             [notify (lambda () (send canvas reshape))]))))
  
  ;;----------------------------------------------------------------------------

  ;; Instantiate the application.

  (define root (new lp-frame%))
  
  ;; Show the application window.

  (send root show #t))

  ;;----------------------------------------------------------------------------
