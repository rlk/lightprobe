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
    (get-ffi-obj "lp_append_image"  lp-lib (_fun _pointer _path -> _void)))
  (define lp-remove-image
    (get-ffi-obj "lp_remove_image"  lp-lib (_fun _pointer _path -> _void)))

  (define lp-set-image-flags
    (get-ffi-obj "lp_set_image_flags" lp-lib
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

  (define lp-render-circle
    (get-ffi-obj "lp_render_circle" lp-lib
      (_fun _pointer _int _int _int _float _float _float _float -> _void)))
  (define lp-render-sphere
    (get-ffi-obj "lp_render_sphere" lp-lib
      (_fun _pointer _int _int _int _float _float _float _float -> _void)))

  ;;----------------------------------------------------------------------------

  (define lightprobe      (lp-init))
  (define lightprobe-path (string->path "Untitled"))

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

  (define drop-frame%
    (class frame%
      (init-field drop-callback)

      (define/override (on-drop-file path)
                       (drop-callback path))

      (super-new)
      (send this accept-drop-files #t)))

  ;;----------------------------------------------------------------------------
  ;; The lightprobe viewer has two modes: Image view and Environment view.
  ;; The mode-options preference group manages the current mode state.

  (define mode-options%
    (class preferences-group%
      (super-new [fraction 0.4])

      ; Interaction callback

      (define (set control event)
        (printf "radio ~s~n" (send radio get-selection)))

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
  ;; The lightprobe viewer has a number of rendering modes. The view-options
  ;; preference group manages these.

  (define view-options%
    (class preferences-group%
      (super-new [fraction 0.4])
      
      ; Interaction callback

      (define (set control event)
        (printf "check ~s~n" (send control get-value)))

      ; GUI sub-elements
      
      (define label (new message%       [parent this]
                                        [label "View:"]))
      (define group (new vertical-pane% [parent this]
                                        [alignment '(left top)]))
      (define grid  (new check-box%     [parent group]
                                        [callback set]
                                        [label "Show Grid"]))
      (define res   (new check-box%     [parent group]
                                        [callback set]
                                        [label "Show Resolution"]))

      ; Public interface

      (define/public (set-grid b) (send grid set-value b))
      (define/public (get-grid)   (send grid get-value))
      (define/public (set-res b)  (send res  set-value b))
      (define/public (get-res)    (send res  get-value))))

  ;;----------------------------------------------------------------------------
  ;; The image-list manages the set of lightprobe images input to the composer.
  ;; It allows the addition and removal of images, hiding and showing of loaded
  ;; images, and the selection of a proper subset of images for configuration.

  (define image-list%
    (class horizontal-pane%
      (super-new)

      ; Private interface

      (define (get-paths)
        (map (lambda (i) (send images get-data i))
                         (send images get-selections)))
      (define (get-index path)
        (send images find-string (path->string path)))

      ; Interaction callbacks

      (define (do-add  control event)
        (map (lambda (p) (send this  add-image p)) (or (get-file-list) '())))
      (define (do-rem  control event)
        (map (lambda (p) (send this  rem-image p)) (get-paths)))
      (define (do-hide control event)
        (map (lambda (p) (send this hide-image p)) (get-paths)))
      (define (do-show control event)
        (map (lambda (p) (send this show-image p)) (get-paths)))

      ; GUI sub-elements

      (define buttons (new vertical-pane% [parent this]
                                          [stretchable-width #f]))
      (define images  (new list-box%      [parent this]
                                          [label #f]
                                          [choices '()]
                                          [style '(extended)]))

      (define add  (instantiate packed-button% ("Add"    buttons do-add)))
      (define rem  (instantiate packed-button% ("Remove" buttons do-rem)))
      (define hide (instantiate packed-button% ("Hide"   buttons do-hide)))
      (define show (instantiate packed-button% ("Show"   buttons do-show)))

      ; Public interface

      (define/public (add-image path)
        (send images append (path->string path) path))

      (define/public (rem-image path)
        (let ((i (get-index path)))
          (if i (send images delete i) #f)))

      (define/public (hide-image path)
        (printf "hide ~s~n" (path->string path)))

      (define/public (show-image path)
        (printf "show ~s~n" (path->string path)))
      ))

  ;;----------------------------------------------------------------------------

  (define lp-menu-bar%
    (class menu-bar%
      (super-new)

      ; File state handlers

      (define (set-lightprobe-path! path)
        (set! lightprobe-path path)
        (send (send this get-frame) set-label (path->string path)))

      (define (set-lightprobe! new path)
        (if new (begin (lp-free new)
                       (set-lightprobe-path! path)
                       (set! lightprobe new) #t) #f))

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
      
      (define/override (on-paint)
        (with-gl-context
         (lambda ()
           (let ((w (send this get-width))
                 (h (send this get-height)))
             (lp-render-sphere #f 0 w h 0.0 0.0 0.0 0.0)
             (swap-gl-buffers)))))
      
      (super-new (style '(gl hscroll vscroll)))))

  ;;----------------------------------------------------------------------------

  (define lp-frame%
    (class drop-frame%
      (super-new [label "Lightprobe Composer"])

      (define menus (new lp-menu-bar% [parent this]))
      ))
  
  ;;----------------------------------------------------------------------------
  ;; The application

  (define root
    (letrec ((frame  (new lp-frame%
                          [drop-callback (lambda (path)
                                           (send images add-image path))]))

             (top    (new horizontal-pane% [parent frame]
                                           [stretchable-height #f]))
             (canvas (new lp-canvas%       [parent frame]
                                           [min-width  640]
                                           [min-height 480]))
             (bot    (new horizontal-pane% [parent frame]
                                           [stretchable-height #f]))

             (img-group (new group-box-panel% [parent top]
                                              [label "Images"]
                                              [stretchable-height #f]))
             (opt-group (new group-box-panel% [parent top]
                                              [label "Options"]
                                              [stretchable-width  #f]))

             (images (new image-list%   [parent img-group]))
             (mode   (new mode-options% [parent opt-group]))
             (view   (new view-options% [parent opt-group]))

           
             (expo (new slider% [parent bot]
                                [label "Exposure"]
                                [min-value   1]
                                [max-value 100]
                                [style '(horizontal plain)]))
             (zoom (new slider% [parent bot]
                                [label "Zoom"]
                                [min-value   1]
                                [max-value 100]
                                [style '(horizontal plain)]))
           
           )
      frame))
  
  ;;----------------------------------------------------------------------------

  (send root show #t))
