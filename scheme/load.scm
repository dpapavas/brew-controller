;; -*- mode:scheme; coding: utf-8; -*-

(import (srfi 1) (srfi 166) (scheme inexact)
        (chibi match)
        (gamma inexact)
        (gamma transformation) (gamma polygons) (gamma polyhedra)
        (gamma operations) (gamma write))

(define-option draft? #false)
(set-curve-tolerance! (if draft? 1/15 1/50))

(define chamfer-length 1/2)
(define wall-thickness 3)

(define base-length 193)
(define base-span 115)
(define base-chamfer 25/2)
(define base-thickness 4)
(define base-height 89)
(define base-width 15)

(define plate-width 137)
(define plate-height 75)

(define face-height 21)
(define tray-height 23)

(define cell-dimensions (list 75 44 10 127/10))
(define cell-mount-offsets (list 2/5 5/2))

(define chamfer-kernel
  (let ((c (* 2 chamfer-length)))
    (square-pyramid c c chamfer-length)))

(define (countersink D)
  (let ((D_eff (* D 6/5)))
    (extrusion
     (circle D_eff)
     (translation 0 0 -50)
     (translation 0 0 0)
     (transformation-append
      (translation 0 0 (/ D_eff 2))
      (scaling 1/2 1/2 1/2))
     (transformation-append
      (translation 0 0 50)
      (scaling 1/2 1/2 1/2)))))

(define-output cell
  (match-let (((a b c d) cell-dimensions))
    (~> (cuboid a d d)
        (apply
         difference
         _
         (list-for ((s (list -1 1))
                    (t (list 0 1)))
           (translate (cylinder 2 50) (+ (* 1/2 s b) (* s t c)) 0 0)))
        (apply
         difference
         _
         (list-for ((s (list -1 1)))
           (~> (cylinder 11/2 50)
               (rotate _ 90 0)
               (translate _ (* s 7/2) 0 0))))
        (translate _ (/ (+ b c) -2) 0 0)
        (flush-bottom _))))

(define (place-board-parts part)
  (list-for ((s (list -1 1))
             (t (list -1 1)))
    (transform part
               (scaling s t 1)
               (translation (* s 254/25) (* t 889/100) 0))))

(define-output board
  (union
   (~> (circle 127/50)
       (linear-extrusion _ 0 8/5)
       (place-board-parts _)
       (apply hull _)
       (apply difference _ (place-board-parts
                            (linear-extrusion (circle 127/100) 50))))

   (~> (cuboid 15 137/20 17/2)
       (flush _ 0 1 -1)
       (translate _ 0 1143/100 8/5))
   (~> (cuboid 15 5/2 17)
       (flush _ 0 -1 -1)
       (rotate _ 90 0)
       (translate _ 0 (- 7/5 1143/100) 8/5))))

(define (place-cell part)
  (~> part
      (rotate _ 180 2)
      (translate _ 0 121/2 0)))

(define (place-board part)
  (~> part
      (rotate _ -90 2)
      (translate _ 0 (+ base-height 0) 0)))

(define-output base
  (match-let* ((l (+ base-span chamfer-length chamfer-length))
               (l/2 (/ l 2))
               (lww (+ l base-width base-width))
               (w (- (/ (- base-length base-span) 2)
                     chamfer-length chamfer-length))
               (τ (- wall-thickness chamfer-length chamfer-length))

               ((a b c d) cell-dimensions)
               (bc (+ b c))
               (abc (- a b c)))
    (~> (rectangle (+ lww (* -2 base-chamfer))
                   (- base-height (/ base-width -2) base-chamfer))
        (flush-south _)
        (minkowski-sum _ (simple-polygon (point (- base-chamfer) 0)
                                         (point base-chamfer 0)
                                         (point 0 base-chamfer)))
        (offset _ (* s base-width))
        (list-for ((s (list 0 -1))) _)
        (apply difference _)
        (difference _ (rectangle l (+ base-width base-width)))

        ;; Front face fillets

        (union _ (let ((r 18/2)
                       (y (- 6 wall-thickness chamfer-length)))
                   (apply
                    union
                    (list-for ((s (list -1 1)))
                      (union
                       (~> (rectangle w y)
                           (flush _ s -1)
                           (translate _ (* -1 s l/2) 0))
                       (~> (rectangle r r)
                           (flush _ s -1)
                           (difference _ (~> (circle r)
                                             (flush _ s -1)
                                             (translate _ 0 0)))
                           (translate _ (* -1 s (+ l/2 base-width)) y)))))))

        (linear-extrusion _ 0 (- base-thickness chamfer-length))

        ;; Cell mount

        (union
         _
         (let* ((ε (first cell-mount-offsets))
                (δ (/ (apply + cell-mount-offsets) 2))
                (r (lambda (x) (+ x 2)))

                (h_1 (+ d (* 2 (+ τ δ chamfer-length))))
                (h_2 (+ d δ δ chamfer-length chamfer-length)))

           (translate
            (difference
             (~> (cuboid lww h_1 base-thickness)
                 (flush-bottom _)
                 (hull _ (~> (cuboid (- lww d d) h_1 (- d chamfer-length))
                             (flush-bottom _)
                             (translate _ 0 0 base-thickness)))
                 (apply
                  union
                  _
                  (list-for ((s (list -1 1)))
                    (~> (cuboid τ 100 (+ base-thickness (- d chamfer-length)))
                        (flush _ s -1 -1)
                        (hull _ (~> (rectangle τ (+ base-thickness (- d chamfer-length)))
                                    (flush _ s 1)
                                    (linear-extrusion _ 0)))
                        (clip _ (plane 0 -1 0 (- (+ base-thickness 6))))
                        (translate _ (* s (+ l/2 τ)) (/ h_1 -2) 0))))
                 (place-cell _)
                 (clip _ (plane 0 -1 0 0)))

             (let ((lbc (- l (- b c)))
                   (h_0 (/ base-thickness 4)))

               (place-cell
                (extrusion
                 (rectangle lbc h_2)
                 (translation 0 0 0)
                 (translation 0 0 h_0)
                 (transformation-append
                  (translation 0 0 (+ (/ lbc 2) h_0))
                  (scaling 2 1 1)))))

             (~> (cuboid 1000 h_2 100)
                 (flush _ 0 0 -1)
                 (translate _ 0 0 (- base-thickness chamfer-length))
                 (place-cell _)))
            0 (- ε δ) 0)))

        ;; Board mount

        (union _ (~> (circle (+ 6/5 127/50))
                     (linear-extrusion _ 0 (- base-thickness chamfer-length))
                     (place-board-parts _)
                     (apply hull _)
                     (apply union _ (~> (circle (- 5/2 chamfer-length))
                                        (linear-extrusion
                                         _ 0 (- base-thickness chamfer-length -5/2))
                                        (place-board-parts _)))
                     (place-board _)))

        (minkowski-sum _ chamfer-kernel)

        ;; Front face

        (union _ (let* ((r 15)

                        (h  (- face-height chamfer-length))
                        (L (- base-length chamfer-length chamfer-length))
                        (part (λ (x z)
                                (~> (cylinder r 50)
                                    (transform _
                                               (rotation 90 0)
                                               (translation x 0 z))))))

                   (~> (cuboid L 3 h)
                       (flush _ 0 1 -1)
                       (difference
                        _
                        (apply
                         hull
                         (part 0 (- l/2 r))
                         (list-for ((s (list -1 1)))
                           (part (* -1 s (- l/2 r)) 0))))

                       (minkowski-sum _ (clip chamfer-kernel
                                              (plane 0 -1 0 0))))))

        (difference
         _
         (place-cell
          (apply union (list-for ((s (list -1 1))
                                  (t (list 0 1)))
                         (transform
                          (countersink 4)
                          (translation (- (* s 5) bc) 0 2/5)
                          (rotation (* t 180) 2)))))

         (~> (countersink 11/4)
             (translate _ 0 0 2/5)
             (place-board-parts _)
             (apply union _)
             (place-board _)))

        ;; Locating screw holes

        (apply
         difference
         _
         (list-for ((s (list -1 1)))
           (let ((r 9/4))
             (translate (union (cylinder r 50)
                               (~> (cylinder 19/4 50)
                                   (flush-bottom _)
                                   (translate _ 0 0 (- base-thickness 4/5))))
                        (* s (+ 111/2 11/2)) (+ 83 11/2 (- wall-thickness)) 0)))))))

(define-output board-cover
  (let* ((ε 1/5)
         (h 3)
         (cc (+ chamfer-length chamfer-length))
         (w (- (* 2/3 wall-thickness) cc))

         (part (λ (x)
                 (~> (circle (+ x 127/50 4/5 cc ε))
                     (linear-extrusion _ (- ε (+ base-thickness 3 8/5)) (+ h x))
                     (place-board-parts _)
                     (apply hull _)))))
    (~> (part w)
        (minkowski-sum _ chamfer-kernel)
        (difference
         _

         (apply union (place-board-parts (cylinder 11/8 100)))
         (apply difference
                (apply union
                       (part 0)
                       (~> (cuboid (+ base-width
                                      chamfer-length
                                      chamfer-length
                                      1) 50 50)
                           (flush _ 0 0 1)
                           (translate _ 0 0 6))
                       (~> (regular-polygon 6 3)
                           (linear-extrusion _ (+ h w 1/2 -1) 50)
                           (place-board-parts _)))
                (~> (apply
                     hull
                     (list-for ((xy (list
                                     (list 0 0)
                                     (list 10 0)
                                     (list 0 10))))
                       (apply translate
                              (circle (- 21/10 chamfer-length)) xy)))
                    (linear-extrusion _ chamfer-length h)
                    (minkowski-sum _ (rotate chamfer-kernel 180 0))
                    (place-board-parts _)))))))

(define-output pin
  (let ((c 1/3))
    (~> (cylinder (- 11/2 c) 5)
        (minkowski-sum _ (hull (cylinder c 0) (point 0 0 (- c))))
        (flush-bottom _)
        (difference _ (countersink 4)))))

(define-output plate
  (let* ((r 5)
         (τ 2)
         (ε 1/5)

         (h_1 1)
         (h_2 7)

         (z (- (- tray-height
                  h_1
                  (fourth cell-dimensions)
                  base-thickness)))

         (part (λ (x) (apply
                       hull
                       (list-for ((s (list -1/2 1/2))
                                  (t (list -1/2 1/2)))
                         (translate (circle x)
                                    (* (- plate-width r r) s)
                                    (* (- plate-height r r) t)))))))
    (~> (union
         (linear-extrusion (part r) (- h_1 ε) (- h_2))
         (linear-extrusion (part 1/2) (- h_1 ε) (+ h_1 1)))

        (difference
         _

         (apply
          hull
          (list-for ((s (list -1/2 1/2)))
            (~> (cylinder h_2 100)
                (transform
                 _
                 (rotation 90 0)
                 (translation (* (- plate-width (* 2 (+ h_2 wall-thickness))) s) 0 (- h_2))))))

         (~> (part r)
             (offset _ (- wall-thickness))
             (linear-extrusion _ 0 -50)))

        (apply
         union
         _

         (apply hull (list-for ((s (list -1/2 1/2)))
                               (~> (circle (/ (fourth cell-dimensions) 2))
                                   (linear-extrusion _ h_1 z)
                                   (translate _ (* (third cell-dimensions) s) 0 0))))

         (let ((part (λ (x y z)
                       (~> (cuboid x wall-thickness z)
                           (flush-top _)
                           (apply difference _ (list-for ((t (list -1/2 1/2)))
                                                 (~> (cylinder z 100)
                                                     (transform
                                                      _
                                                      (rotation 90 0)
                                                      (translation
                                                       (* (+ x (/ z 2)) t) 0 (- z))))))
                           (translate _ 0 y 0)))))
           (append
            (list-for ((s (list -1/2 1/2)))
              (part (* plate-width 8/9) (+ (* s 5/2 (fourth cell-dimensions)) 1) h_2))
            (list-for ((s (list -7/16 -7/32 7/32 7/16)))
              (rotate (part (* plate-height 1) (* s base-span) (/ h_2 2)) 90 2)))))

        (apply difference _ (list-for ((s (list -1/2 1/2)))
                              (transform (countersink 4)
                                         (rotation 180 0)
                                         (translation (* (third cell-dimensions) s) 0 h_1))))
        (translate _ 0 0 (- z)))))

(define-output plate-frame
  (let* ((r 10)
         (a 283/2)
         (b 78)
         (τ 1)

         (upper (λ (x) (apply hull (list-for ((s (list -1/2 1/2))
                                              (t (list -1/2 1/2)))
                                     (~> (sphere (+ r x (- chamfer-length)))
                                         (clip _ (plane 0 0 1 (* r 1/4)))
                                         (translate _ (* s a) (* t b) (- r chamfer-length)))))))
         (lower (λ (x) (apply hull (list-for ((s (list -1/2 1/2))
                                              (t (list -1/2 1/2)))
                                       (let ((ε (+ -10 4/5)))
                                         (~> (cylinder (+ 5 x) (- 2 chamfer-length))
                                             (flush-top _)
                                             (translate _
                                                        (* s (+ plate-width ε))
                                                        (* t (+ plate-height ε)) 0))))))))

    (difference
     (minkowski-sum
      chamfer-kernel
      (difference
       (hull
        (upper τ)
        (lower 6))
       (upper 0)))
     (translate (lower 0) 0 0 (- chamfer-length 1))
     (transform (lower -9/2) (translation 0 0 1/2) (scaling 1 1 10)))))

(define-output assembly
  (union base
         (place-cell (translate cell 0 0 base-thickness))
         (place-cell (translate plate 0 0 (+ base-thickness (fourth cell-dimensions))))
         (place-cell (translate plate-frame 0 0 (+ tray-height 1/2)))
         (place-cell (~> (apply cuboid (map (λ (x) (- x chamfer-length chamfer-length))
                                            (list 193 127 26)))
                         (minkowski-sum _ chamfer-kernel)
                         (flush-bottom _)
                         (translate _ 0 0 tray-height)))
         (place-board (translate board 0 0 (+ base-thickness 5/2)))
         #;(place-board (~> board-cover
                          #;(clip _ (plane -1 0 0 0))
                          (translate _ 0 0 (+ base-thickness 5/2 8/5))))))
