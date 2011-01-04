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

  ;;----------------------------------------------------------------------------
  ;; Rendering and image processing are performed by the graphics hardware via
  ;; the OpenGL Shading Language. This is managed by the lightprobe library, a
  ;; dynamically-loaded native code module written in C. The following FFI
  ;; binds that library API.

  (require ffi/unsafe)

  (define lp-lib (ffi-lib "lp-render"))

  (define (lp-ffi str sig)
    (get-ffi-obj str lp-lib sig))

  (define lp-init
    (lp-ffi "lp_init" (_fun          -> _pointer)))
  (define lp-free
    (lp-ffi "lp_free" (_fun _pointer -> _void)))

  (define lp-add-image
    (lp-ffi "lp_add_image" (_fun _pointer _path -> _int)))
  (define lp-del-image
    (lp-ffi "lp_del_image" (_fun _pointer _int  -> _void)))

  (define lp-get-image-width
    (lp-ffi "lp_get_image_width"  (_fun _pointer _int -> _int)))
  (define lp-get-image-height
    (lp-ffi "lp_get_image_height" (_fun _pointer _int -> _int)))

  (define lp-image-loaded 1)
  (define lp-image-active 2)
  (define lp-image-hidden 4)

  (define lp-set-image-flags
    (lp-ffi "lp_set_image_flags" (_fun _pointer _int _int -> _void)))
  (define lp-clr-image-flags
    (lp-ffi "lp_clr_image_flags" (_fun _pointer _int _int -> _void)))
  (define lp-get-image-flags
    (lp-ffi "lp_get_image_flags" (_fun _pointer _int      -> _int)))

  (define lp-circle-x         0)
  (define lp-circle-y         1)
  (define lp-circle-radius    2)
  (define lp-sphere-elevation 3)
  (define lp-sphere-azimuth   4)
  (define lp-sphere-roll      5)

  (define lp-set-image-value
    (lp-ffi "lp_set_image_value" (_fun _pointer _int _int _float -> _void)))
  (define lp-get-image-value
    (lp-ffi "lp_get_image_value" (_fun _pointer _int _int        -> _float)))

  (define lp-render-grid 1)
  (define lp-render-res  2)

  (define lp-render-circle
    (lp-ffi "lp_render_circle" 
      (_fun _pointer _int _int _int _float _float _float _float -> _void)))
  (define lp-render-sphere
    (lp-ffi "lp_render_sphere" 
      (_fun _pointer _int _int _int _float _float _float _float -> _void)))

  (define lp-export-cube
    (lp-ffi "lp_export_cube"   (_fun _pointer _path -> _bool)))
  (define lp-export-sphere
    (lp-ffi "lp_export_sphere" (_fun _pointer _path -> _bool)))

  ;;----------------------------------------------------------------------------

  (define (lp-is-image? i b)
    (and (not (negative? i))
         (not (zero? (bitwise-and b (lp-get-image-flags lightprobe i))))))

  (define (lp-is-image-loaded? i) (lp-is-image? i lp-image-loaded))
  (define (lp-is-image-active? i) (lp-is-image? i lp-image-active))
  (define (lp-is-image-hidden? i) (lp-is-image? i lp-image-hidden))

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
                                           [callback (lambda x notify)]
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
                                        [callback (lambda x notify)]))
      (define res   (new check-box%     [parent group]
                                        [label "Show Resolution"]
                                        [callback (lambda x notify)]))

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

      ; GUI sub-elements

      (define expo (new slider% [parent this]
                                [label "Exposure"]
                                [min-value  0]
                                [max-value  8]
                                [style '(horizontal plain)]
                                [callback (lambda x notify)]))
      (define zoom (new slider% [parent this]
                                [label "Zoom"]
                                [min-value -5]
                                [max-value  5]
                                [style '(horizontal plain)]
                                [callback (lambda x notify)]))

      ; Public interface

      (define/public (get-expo)
        (exact->inexact (send expo get-value)))

      (define/public (get-zoom)
        (let ((z (expt 2 (send zoom get-value))))
          (exact->inexact z)))
      ))

  ;;----------------------------------------------------------------------------
  ;; The image-list manages the set of lightprobe images input to the composer.
  ;; It allows the addition and removal of images, hiding and showing of loaded
  ;; images, and the selection of a subset of images for configuration.

  (define image-list%
    (class horizontal-pane%
      (super-new)
      (init-field notify)

      ; The "index" is the image's GUI list-box position.

      (define (get-index string)
        (send images find-string string))
      
      ; The "descr" is the image's lightprobe API descriptor.
    
      (define (get-descr string)
        (send images get-data (get-index string)))

      (define (set-flag string flag)
        (lp-set-image-flags lightprobe (get-descr string) flag) (void))
      (define (clr-flag string flag)
        (lp-clr-image-flags lightprobe (get-descr string) flag) (void))

      ; Interaction callbacks

      (define (get-file-strings)
        (map path->string (or (get-file-list) '())))

      (define (do-add  control event)
        (map (lambda (s) (send this  add-image s)) (get-file-strings)))
      (define (do-del  control event)
        (map (lambda (s) (send this  del-image s)) (get-selected-list)))
      (define (do-hide control event)
        (map (lambda (s) (set-flag s lp-image-hidden)) (get-selected-list)))
      (define (do-show control event)
        (map (lambda (s) (clr-flag s lp-image-hidden)) (get-selected-list)))

      ; Set the 'active' flag on all images based on selection status.

      (define (set control event)
        (map (lambda (s)
               (if (send images is-selected? (get-index s))
                   (set-flag s lp-image-active)
                   (clr-flag s lp-image-active)))
             (get-loaded-list)))

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

      (define/public (add-image string)
        (let ((d (lp-add-image lightprobe string)))

          (if (lp-is-image-loaded? d)
              (begin (send images append string d)
                     (notify))
              (void))))

      ; Remove (unload) the named image.

      (define/public (del-image string)
        (begin (lp-del-image lightprobe (get-descr string))
               (send images delete      (get-index string))
               (notify)))

      ; Write the current image state to the named file.

      (define/public (save-file path)
        #f)

      ; Load the named file to the current image state.

      (define/public (load-file path)
        #f)

      ; Unload all currently-loaded images.

      (define/public (init-file)
        #f)

      ; Return a list of path strings of all currently-selected images.

      (define/public (get-selected-list)
        (map (lambda (i) (send images get-string i))
                         (send images get-selections)))

      ; Return a list of path strings of all currently-loaded images.

      (define/public (get-loaded-list)
        (build-list   (send images get-number)
          (lambda (i) (send images get-string i))))))

  ;;----------------------------------------------------------------------------

  (define lp-menu-bar%
    (class menu-bar%
      (super-new)
      (init-field save-file)
      (init-field load-file)
      (init-field init-file)

      ; File / New

      (define (do-new control event)
        (set! lightprobe-path default-path)
        (init-file))

      ; File Save

      (define (do-save control event)
        (save-file lightprobe-path))

      ; File / Open

      (define (do-open control event)
        (let ((path (get-file)))
          (if path
              (begin
                (set! lightprobe-path path)
                (init-file)
                (load-file lightprobe-path))
              (void))))

      ; File / Save As...

      (define (do-save-as control event)
        (let ((path (put-file)))
          (if path
              (begin
                (set! lightprobe-path path)
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

  (define lp-canvas%
    (class canvas%
      (inherit refresh with-gl-context swap-gl-buffers)

      (init-field get-mode)
      (init-field get-paths)
      (init-field get-expo)
      (init-field get-zoom)
      (init-field get-grid)
      (init-field get-res)

      (define (get-x)
        (let-values (((x y) (send this get-view-start))
                     ((w h) (send this get-virtual-size)))
          (exact->inexact (/ x w))))
      (define (get-y)
        (let-values (((x y) (send this get-view-start))
                     ((w h) (send this get-virtual-size)))
          (exact->inexact (/ y h))))

      (define/override (on-paint)
        (if lightprobe
          (with-gl-context
            (lambda ()
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

                (swap-gl-buffers))))
          (void)))

      (define/public (reshape)
        (let* ((ps (filter (not lp-is-image-hidden?) (get-paths)))

               (ws (map (lambda (p) (lp-get-image-width  lightprobe p)) ps))
               (hs (map (lambda (p) (lp-get-image-height lightprobe p)) ps))

               (zoom (lambda (d) (inexact->exact (ceiling (* d (get-zoom))))))

               (w (if (empty? ws) 1 (zoom (apply max ws))))
               (h (if (empty? hs) 1 (zoom (apply max hs))))
               (x (get-x))
               (y (get-y)))

          (send this init-auto-scrollbars w h x y)))

      (super-new (style '(gl hscroll vscroll)))))

  ;;----------------------------------------------------------------------------

  ;; The lp-frame is the toplevel window of the application, and the overall
  ;; organizing structure through which all state changes flow. All GUI sub-
  ;; groups are instantiated as members and a variety of anonymous functions
  ;; are closed within this scope, providing cross-cutting access between
  ;; sub-elements of the application.

  (define lp-frame%
    (class drop-frame%
      (super-new [label "Lightprobe Composer"]
                 [drop-callback
                  (lambda (path) (send images add-image (path->string path)))])

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
             [save-file (lambda (path) (send images load-file path))]
             [load-file (lambda (path) (send images open-file path))]
             [init-file (lambda ()     (send images init-file))]))

      (define canvas
        (new lp-canvas%
             [parent mid]
             [min-width  640]
             [min-height 480]
             [get-paths (lambda () (send images  get-loaded-list))]
             [get-mode  (lambda () (send mode    get-mode))]
             [get-expo  (lambda () (send values  get-expo))]
             [get-zoom  (lambda () (send values  get-zoom))]
             [get-grid  (lambda () (send options get-grid))]
             [get-res   (lambda () (send options get-res))]))

      (define images
        (new image-list%
             [parent img]
             [notify    (lambda () (send canvas reshape))]))

      (define mode
        (new view-mode%
             [parent opt]
             [notify    (lambda () (send canvas reshape))]))

      (define options
        (new view-options%
             [parent opt]
             [notify    (lambda () (send canvas refresh))]))

      (define values
        (new view-values%
             [parent bot]
             [notify    (lambda () (send canvas refresh))]))))
  
  ;;----------------------------------------------------------------------------

  ;; Instantiate the application.

  (define root (new lp-frame%))
  
  ;; Show the application window, activating the OpenGL context.

  (send root show #t)

  ;; With the OpenGL context active, instantiate the lightprobe.

  (set! lightprobe (lp-init)))

  ;;----------------------------------------------------------------------------


