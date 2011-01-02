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
  ;; Rendering and image processing are performed by the graphics hardware via
  ;; the OpenGL Shading Language. This is managed by the lightprobe library, a
  ;; dynamically-loaded native code module written in C. The following FFI
  ;; binds that library API.

  (require ffi/unsafe)

  (define lp-lib (ffi-lib "/Users/rlk/Projects/lightprobe/lp-render"))

  (define lp-init
    (get-ffi-obj "lp_init"          lp-lib (_fun                -> _pointer)))
  (define lp-open
    (get-ffi-obj "lp_open"          lp-lib (_fun          _path -> _pointer)))
  (define lp-save
    (get-ffi-obj "lp_save"          lp-lib (_fun _pointer _path -> _bool)))
  (define lp-free
    (get-ffi-obj "lp_free"          lp-lib (_fun _pointer       -> _void)))
  (define lp-export-cube
    (get-ffi-obj "lp_export_cube"   lp-lib (_fun _pointer _path -> _bool)))
  (define lp-export-sphere
    (get-ffi-obj "lp_export_sphere" lp-lib (_fun _pointer _path -> _bool)))

  (define lp-append-image
    (get-ffi-obj "lp_append_image"  lp-lib (_fun _pointer _path -> _bool)))
  (define lp-remove-image
    (get-ffi-obj "lp_remove_image"  lp-lib (_fun _pointer _path -> _bool)))

  (define lp-get-image-width
    (get-ffi-obj "lp_get_image_width"  lp-lib (_fun _pointer _path -> _int)))
  (define lp-get-image-height
    (get-ffi-obj "lp_get_image_height" lp-lib (_fun _pointer _path -> _int)))

  (define lp-image-active 1)
  (define lp-image-hidden 2)

  (define lp-set-image-flags
    (get-ffi-obj "lp_set_image_flags" lp-lib
      (_fun _pointer _path _int -> _void)))
  (define lp-clr-image-flags
    (get-ffi-obj "lp_clr_image_flags" lp-lib
      (_fun _pointer _path _int -> _void)))
  (define lp-get-image-flags
    (get-ffi-obj "lp_get_image_flags" lp-lib
      (_fun _pointer _path      -> _int)))

  (define lp-move-circle
    (get-ffi-obj "lp_move_circle" lp-lib
      (_fun _pointer _float _float _float -> _void)))
  (define lp-move-sphere
    (get-ffi-obj "lp_move_sphere" lp-lib
      (_fun _pointer _float _float _float -> _void)))

  (define lp-render-grid 1)
  (define lp-render-res  2)

  (define lp-render-circle
    (get-ffi-obj "lp_render_circle" lp-lib
      (_fun _pointer _int _int _int _float _float _float _float -> _void)))
  (define lp-render-sphere
    (get-ffi-obj "lp_render_sphere" lp-lib
      (_fun _pointer _int _int _int _float _float _float _float -> _void)))

  ;;----------------------------------------------------------------------------

  (define lightprobe #f)
  (define lightprobe-path (string->path "Untitled"))

  (define (set-lightprobe! new path)
    (if new
        (begin
          (set-lightprobe-path! path)
          (lp-free lightprobe)
          (set!    lightprobe new)
          (void))
        (void)))

  (define (set-lightprobe-path! path)
    (set! lightprobe-path path)
    (send root set-label (path->string path)))

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
      (super-new [vert-margin 0] [horiz-margin 0])))

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
  ;; The view-mode preference group manages the current mode state, with the
  ;; lightprobe canvas as observer.

  (define view-mode%
    (class preferences-group%
      (super-new [fraction 0.4])
      (init-field observer)

      ; Interaction callback

      (define (set control event)
        (send observer refresh))

      ; GUI sub-elements
      
      (define label (new message%          [parent this]
                                           [label "Mode:"]))
      (define radio (new simple-radio-box% [parent this]
                                           [callback set]
                                           [choices '("Image" "Environment")]))
      ; Public interface

      (define/public (set-mode n) (send radio set-selection n))
      (define/public (get-mode)   (send radio get-selection))))

  ;;----------------------------------------------------------------------------
  ;; The lightprobe viewer has a number of rendering options. The view-options
  ;; preference group manages these, with the lightprobe canvas as observer.

  (define view-options%
    (class preferences-group%
      (super-new [fraction 0.4])
      (init-field observer)
      
      ; Interaction callback

      (define (do-grid control event) (set-grid (send control get-value)))
      (define (do-res  control event) (set-res  (send control get-value)))

      ; GUI sub-elements
      
      (define label (new message%       [parent this]
                                        [label "View:"]))
      (define group (new vertical-pane% [parent this]
                                        [alignment '(left top)]))
      (define grid  (new check-box%     [parent group]
                                        [callback do-grid]
                                        [label "Show Grid"]))
      (define res   (new check-box%     [parent group]
                                        [callback do-res]
                                        [label "Show Resolution"]))

      ; Public interface

      (define/public (set-grid b)
        (send grid set-value b)
        (send observer refresh))
      (define/public (get-grid)
        (send grid get-value))

      (define/public (set-res b)
        (send res set-value b)
        (send observer refresh))
      (define/public (get-res)
        (send res get-value))))

  ;;----------------------------------------------------------------------------
  ;; The lightprobe viewer has a few variables that tune the rendering. The
  ;; view-values group manages these, with the lightprobe canvas as observer.

  (define view-values%
    (class horizontal-pane%
      (super-new)
      (init-field observer)

      (define (set control event)
        (send observer refresh))

      (define expo (new slider% [parent this]
                                [label "Exposure"]
                                [min-value  0]
                                [max-value  8]
                                [callback set]
                                [style '(horizontal plain)]))
      (define zoom (new slider% [parent this]
                                [label "Zoom"]
                                [min-value -5]
                                [max-value  5]
                                [callback set]
                                [style '(horizontal plain)]))

      (define/public (get-expo)
        (send expo get-value))
      (define/public (get-zoom)
        (expt 2 (send zoom get-value)))))

  ;;----------------------------------------------------------------------------
  ;; The image-list manages the set of lightprobe images input to the composer.
  ;; It allows the addition and removal of images, hiding and showing of loaded
  ;; images, and the selection of a subset of images for configuration.

  (define image-list%
    (class horizontal-pane%
      (super-new)
      (init-field observer)

      ; Private interface

      (define (get-selection-list)
        (map (lambda (i) (send images get-data i))
                         (send images get-selections)))

      (define (get-index path)
        (send images find-string (path->string path)))

      (define (append-path path)
        (send images append (path->string path) path))

      (define (remove-path path)
        (let ((i (get-index path)))
          (if i (send images delete i) (void))))

      ; Interaction callbacks

      (define (do-add  control event)
        (map (lambda (p) (send this  add-image p)) (or (get-file-list) '())))
      (define (do-rem  control event)
        (map (lambda (p) (send this  rem-image p)) (get-selection-list)))
      (define (do-hide control event)
        (map (lambda (p) (send this hide-image p)) (get-selection-list)))
      (define (do-show control event)
        (map (lambda (p) (send this show-image p)) (get-selection-list)))

      (define (set control event)
        (let loop ((n (send images get-number)) (i 0))
          (if (< i n)
              (let ((path (send images get-data i)))
                (if (send images is-selected? i)
                    (lp-set-image-flags lightprobe path lp-image-active)
                    (lp-clr-image-flags lightprobe path lp-image-active))
                (loop n (+ i 1)))
              (void))))

      ; GUI sub-elements

      (define buttons (new vertical-pane% [parent this]
                                          [stretchable-width #f]))
      (define images  (new list-box%      [parent this]
                                          [label #f]
                                          [choices '()]
                                          [callback set]
                                          [style '(extended)]))

      (define add  (instantiate packed-button% ("Add"    buttons do-add)))
      (define rem  (instantiate packed-button% ("Remove" buttons do-rem)))
      (define hide (instantiate packed-button% ("Hide"   buttons do-hide)))
      (define show (instantiate packed-button% ("Show"   buttons do-show)))

      ; Public interface

      (define/public (add-image path)
        (if (lp-append-image lightprobe path)
          (begin
            (append-path path)
            (send observer reshape))
          (void)))

      (define/public (rem-image path)
        (if (lp-remove-image lightprobe path)
          (begin
            (remove-path path)
            (send observer reshape))
          (void)))

      (define/public (hide-image path)
        (lp-set-image-flags lightprobe path lp-image-hidden)
        (send observer reshape))

      (define/public (show-image path)
        (lp-clr-image-flags lightprobe path lp-image-hidden)
        (send observer reshape))

      (define/public (get-path-list)
        (build-list (send images get-number)
                    (lambda (i) (send images get-data i))))))

  ;;----------------------------------------------------------------------------

  (define lp-menu-bar%
    (class menu-bar%
      (super-new)

      ; File state handlers

      (define (save path)
        (set-lightprobe-path! path)
        (lp-save lightprobe   path))

      ; File / New

      (define (do-new control event)
        (set-lightprobe! (lp-init) (string->path "Untitled")))

      ; File / Open

      (define (do-open control event)
        (let ((path (get-file)))
          (if path
            (set-lightprobe! (lp-open path) path) #f)))

      ; File Save

      (define (do-save control event)
        (save lightprobe-path))

      ; File / Save As...

      (define (do-save-as control event)
        (let ((path (put-file)))
          (if path
            (save path) #f)))

      ; File / Export Cube...

      (define (do-export-cube control event)
        (let ((path (put-file)))
          (if path (lp-export-cube lightprobe path) #f)))

      ; File / Export Sphere...

      (define (do-export-sphere control event)
        (let ((path (put-file)))
          (if path (lp-export-sphere lightprobe path) #f)))

      ; Menus and menu items.

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
                        [shortcut-prefix '(shift cmd)])

        (new separator-menu-item% [parent file])

        (new menu-item% [parent file]
                        [label "Export Cube Map..."]
                        [callback do-export-cube]
                        [shortcut #\e])
        (new menu-item% [parent file]
                        [label "Export Sphere Map..."]
                        [callback do-export-sphere]
                        [shortcut #\e]
                        [shortcut-prefix '(shift cmd)]))))

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

      (define/override (on-paint)
        (if lightprobe
          (with-gl-context
            (lambda ()
              (let ((w (send this get-width))
                    (h (send this get-height)))

                (lp-render-circle lightprobe 0 w h 0.0 0.0 0.0 0.0)
                (swap-gl-buffers))))
          (void)))

#|
      (define (update)
        (let* ((ps (get-all-paths))
               (ws (map (lambda (p) (lp-get-image-width  lightprobe p)) ps))
               (hs (map (lambda (p) (lp-get-image-height lightprobe p)) ps))
               (w  (if (empty? ws) 1 (apply max ws)))
               (h  (if (empty? hs) 1 (apply max hs))))
          (send observer reshape w h)))


      (define/public (reshape w h)
        (let ((x (send this get-scroll-pos 'horizontal))
              (y (send this get-scroll-pos 'vertical)))
          (send this init-auto-scrollbars w h x y)))
|#      
      (define/public (reshape)
        (send this refresh))

      (super-new (style '(gl hscroll vscroll)))))

  ;;----------------------------------------------------------------------------

  ;; The lp-frame is the toplevel window of the application, and the overall
  ;; organizing structure through which all state changes flow. All GUI sub-
  ;; groups are instantiated as members and a variety of anonymous functions
  ;; a closed within this scope, providing cross-cutting access between sub-
  ;; elements of the application.

  (define lp-frame%
    (class drop-frame%
      (super-new [label "Lightprobe Composer"]
                 [drop-callback (lambda (path)
                                  (send images add-image path))])

      (define menus (new lp-menu-bar% [parent this]))

      (define top (new horizontal-pane% [parent this] [stretchable-height #f]))
      (define mid (new horizontal-pane% [parent this] [stretchable-height #t]))
      (define bot (new horizontal-pane% [parent this] [stretchable-height #f]))

      (define img (new group-box-panel% [parent top]
                                        [label "Images" ]
                                        [stretchable-width #t]))
      (define opt (new group-box-panel% [parent top]
                                        [label "Options"]
                                        [stretchable-width #f]))

      (define canvas
        (new lp-canvas%
             [parent     mid]
             [min-width  640]
             [min-height 480]
             [get-mode  (lambda () (send mode    get-mode))]
             [get-paths (lambda () (send images  get-path-list))]
             [get-expo  (lambda () (send values  get-expo))]
             [get-zoom  (lambda () (send values  get-zoom))]
             [get-grid  (lambda () (send options get-grid))]
             [get-res   (lambda () (send options get-res))]))

      (define images  (new image-list%   [parent img] [observer canvas]))
      (define mode    (new view-mode%    [parent opt] [observer canvas]))
      (define options (new view-options% [parent opt] [observer canvas]))
      (define values  (new view-values%  [parent bot] [observer canvas]))))
  
  ;;----------------------------------------------------------------------------

  ;; Instantiate the application.

  (define root (new lp-frame%))
  
  ;; Show the application window.

  (send root show #t)

  ;; Start up with a new document.

  (set-lightprobe! (lp-init) (string->path "Untitled")))