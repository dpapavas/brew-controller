;; -*- mode:scheme; coding: utf-8 -*-

;; Copyright 2024 Dimitris Papavasiliou

;; This program is free software: you can redistribute it and/or modify
;; it under the terms of the GNU Affero General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU Affero General Public License for more details.

;; You should have received a copy of the GNU Affero General Public License
;; along with this program.  If not, see <https://www.gnu.org/licenses/>.

(import (srfi 1) (srfi 166) (scheme inexact)
        (gamma inexact)
        (gamma transformation) (gamma polygons) (gamma polyhedra)
        (gamma operations) (gamma write))

(define-option draft? #false)
(set-curve-tolerance! (if draft? 1/15 1/200))

(define enclosure-height 115)
(define enclosure-width 95)
(define enclosure-depth 27)
(define enclosure-face-depth 46)
(define enclosure-face-inset 13/2)
(define clip-angle 15)

(define mount-boss-radius (+ 5/2 6/5))
(define mount-boss-height 7)
(define mount-plate-width 6/5)
(define mount-gasket-width 1)
(define mount-boss-elevation -1/2)

(define wall-width 5/2)
(define pcb-boss-height 4)

(print-note
 (show #f "Mount width: "
       (with ((precision 1))
             (/ (- enclosure-width enclosure-face-inset (* -2 wall-width))
                (cos-degrees clip-angle)))))

(print-note
 (show #f "Mount heigth: "
       (with ((precision 1)) (+ enclosure-height (* 2 wall-width)))))

(define chamfer-kernel
  #;(let* ((r 3/2)
         (l (* 2 (- wall-width r))))
    (minkowski-sum (cuboid l l l) (sphere r)))
  (sphere wall-width)
  #;(let ((c (* 2 wall-width)))
    (octahedron c c wall-width)))

(define (mount-plane s)
  (~> (plane 0 0 s 0)
      (rotate _ clip-angle 0)
      (translate _ 0 0 enclosure-depth)))

(define (bracket l h D staggered?)
  (let* ((c 1/5)
         (r (- D c))
         (r_1 (* 2/3 r)))
    (minkowski-sum
     (hull (linear-extrusion (circle c) 0) (point 0 0 c))
     (apply
      union

      (~> (rectangle (- h r_1) (- l r_1))
          (when~> _
                  staggered?
                  (difference _ (~> (rectangle 50 50)
                                    (minkowski-sum _ (regular-polygon 4 r))
                                    (flush _ -1 -1)
                                    (translate _
                                               (/ (- h r_1) 6)
                                               (/ (- l r_1) -6)))))
          (minkowski-sum _ (simple-polygon
                            (point 0 r_1)
                            (point 0 (- r_1))
                            (point r_1 0)))
          (offset _ (* δ r_1))
          (linear-extrusion _ r_1)
          (rotate _ -90 1)
          (list-for ((δ (list 0 -1))) _)
          (apply difference _)

          (flush _ 1 0 -1)
          (difference _  (flush (cuboid r_1 (- l r_1) r) 1 0 -1))
          (translate _ (- (+ D c)) 0 0))

      (list-for ((s (list -1 1)))
        (let ((w 2)
              (ε 1/10))
          (~> (~> (rectangle r_1 r_1)
                  (flush-east _)
                  (translate _ (- (+ D c)) 0))
              (apply union _ (list-for ((t (list -1/4 1/4)))
                               (translate (circle r) 0 (* t r))))
              (hull _)
              (linear-extrusion _ w)
              (flush-bottom _)
              (difference _ (cylinder (+ (/ D 2) ε c) 50))
              (translate _ 0 (* 1/2 s l) 0))))))))

(define-output psu-bracket (bracket 17 25 3 #false))
(define-output controller-bracket (bracket 706/10 32 5/2 #true))

(define (countersink D h outer?)
  (let ((D_eff (* D 11/10)))
    (when~> (list
             (translation 0 0 -50)
             (translation 0 0 (- (* D_eff -2/3) wall-width))
             (transformation-append
              (translation 0 0 (- (* D_eff 1/3) wall-width))
              (scaling 1/2 1/2 1/2))
             (transformation-append
              (translation 0 0 (- h (/ D_eff 2)))
              (scaling 1/2 1/2 1/2))
             (transformation-append
              (translation 0 0 h)
              (scaling 1/4 1/4 1/4))
             (transformation-append
              (translation 0 0 (+ h 5))
              (scaling 1/4 1/4 1/4)))
            (not outer?) (drop-right _ 1)
            #true (apply extrusion (circle (* 2 D_eff)) _)
            (not outer?) (minkowski-sum (linear-extrusion
                                         (circle (* 1/2 wall-width)) 0) _))))

(define (thread-cutout D P L pointy?)
  (let* ((H (* 1/2 (sqrt 3) P))
         (r (- (/ D 2) (* 7/8 H))))

    (~> (difference
         (simple-polygon
          (point (* -1/2 P) 0)
          (point (* 1/2 P) 0)
          (point 0 H))

         ;; Shave off a bit (H/16) of the thread; the standard allows
         ;; it (not that it matters) and it ensures a valid resulting
         ;; mesh.

         (rectangle P (/ H 8)))

        (apply extrusion
               _
               (list-for ((s (iota (- (ceiling (/ L P)) (if pointy? 1 0))))
                          (t (iota 30 0 1/30)))
                 (transformation-append
                  (translation 0 0 (/ P 2))
                  (rotation -90 1)
                  (rotation (* 360 t) 0)
                  (translation 0 r 0)
                  (translation (* P (+ s t)) 0 0))))

        (union _ (extrusion (regular-polygon 30 (+ r (/ H 8)))
                            (scaling 4/3 4/3 1)
                            (translation 0 0 (/ r 3))
                            (translation 0 0 L)
                            (let ((c (if pointy? 1/100 10/6)))
                              (transformation-append
                               (translation 0 0 (+ L (/ P 2) (* r 6/10)))
                               (scaling c c 1)))))

        (clip _ (plane 0 0 -1 0)))))

(define (add-base-fillet part r)
  (union
   part
   (~> part
       (clip _ (plane 0 0 1 -1/10))
       (minkowski-sum _ (~> (rectangle r r)
                            (flush _ -1 -1)
                            (difference
                             _
                             (flush
                              (scale (circular-sector r 90) -1 -1) -1 -1))
                            (translate _ (- r) 0)
                            (angular-extrusion _ r 360)
                            (rotate _ 90 0)))
       (translate _ 0 0 -1/10))))

(define (thread-boss D H L_1 L_2 outer?)
  (let* ((c 1/2)
         (r (* 1/2 (+ (/ (- D H) 2) c)))
         (ε 3/20))
    (flush-top
     (if outer?
         (cylinder (+ (/ H 2) ε) L_2)
         (~> (hull
              (flush-bottom (cylinder (/ D 2) L_1))
              (flush-bottom (cylinder (+ (/ D 2) c) (- L_1 c))))
             (apply
              union
              _
              (list-for ((θ (list 0 90)))
                (let ((w 1)
                      (h (- (min L_1 5) (* 3/2 c))))
                  (~> (hull (flush-bottom (cuboid D w h))
                            (flush-bottom (cuboid (+ D h h) w r)))
                      (minkowski-sum _ (square-pyramid c c (/ c 2)))
                      (rotate _ θ 2)))))

             (add-base-fillet _ r))))))

(define (display-boss outer?)
  (if (not outer?)
      (thread-boss 27/5 4 2 0 #false)

      (translate
       (union
        (~> (list-for ((u (list 0 1)))
              (translate
               (regular-polygon 6 13/5) 0 (* -1 u (+ 12/5 9/2))))
            (apply hull _)
            (linear-extrusion _ 17/10))
        (~> (list-for ((u (list 0 1)))
              (translate (circle 5/4) 0 (* -1 u 5)))
            (apply hull _)
            (linear-extrusion _ 0 5))
        (~> (cuboid 6 6 10)
            (flush-bottom _)
            (translate _ 0 -10 0))) 0 0 -2)))

(define-output nut
  (~> (prism 6 127/20 5)
      (intersection _ (cylinder 61/10 5))
      (intersection _ (translate (sphere 7) 0 0 -9/5))
      (flush-bottom _)
      (union _ (~> (cylinder 34/5 4/5)
                   (minkowski-sum _ (extrusion
                                     (regular-polygon 4 1/5)
                                     (rotation 90 0)))
                   (flush-bottom _)
                   (hull _ (extrusion (circle 11/2)
                                      (translation 0 0 2)))))
      (difference _ (thread-cutout 7 3/4 32/5 #false))))

(define-output controller
  (union (~> (apply hull
                    (list-for ((s (list -1/2 1/2))
                               (t (list -1/2 1/2)))
                      (translate (cylinder 3 8/5)
                                 (* s 706/10) (* t 371/10) 0)))
             (apply difference
                    _
                    (list-for ((s (list -1/2 1/2))
                               (t (list -1/2 1/2)))
                      (translate (cylinder 27/20 8/5)
                                 (* s 706/10) (* t 371/10) 0)))
             (flush-bottom _))

         ;; Teensy

         (~> (cuboid 18 35 25/2)
             (flush _ 0 -1 -1)
             (union _ (~> (cuboid 8 6 3)
                          (flush-bottom _)
                          (translate _ 0 3/2 25/2)))
             (flush-west _)
             (translate _ (- 43/5 767/20) (- 8/5 431/20) 8/5))

         ;; MAX31865

         (~> (apply hull
                    (list-for ((s (list -1/2 1/2))
                               (t (list -1/2 1/2)))
                      (translate (cylinder 3 8/5)
                                 (* s (- 25 6)) (* t (- 28 6)) 0)))

             (apply difference
                    _
                    (list-for ((s (list -1/2 1/2)))
                      (translate (cylinder 3/2 8/5)
                                 (* s 81/4) 113/10 0)))

             (flush _ 0 -1 -1)
             (union _ (~> (cuboid 15 7 9)
                          (flush _ 0 -1 -1)
                          (translate _ 0 21 8/5)))
             (flush-west _)
             (translate _ (- 5 767/20) (- 431/20 5) 13))

         ;; Heat sinks

         (apply union
                (list-for ((s (list -1 1)))
                  (~> (cuboid 46 10 20)
                      (flush _ 0 s -1)
                      (translate _
                                 (- 61/4 0) (* -1 s 431/20) (+ 8/5 1)))))))

(define-output psu
  (flush-bottom (cuboid 65 25 20)))

(define-output display
  (flush-top
   (union
    (cuboid 89/2 37 (+ 6 8/5))
    (translate (cuboid 34 34 2) 0 -3/2 24/5))))

(define (spline δ)
  (let* ((c 3/5)
         (r (+ 6/2 δ))
         (rr (+ r r)))
    (difference
       (apply
        extrusion
        (circle r)
        (when~> (list
                 (translation 0 0 c)
                 (translation 0 0 13))
                (positive? δ) (cons (transformation-append
                                     (translation 0 0 (- c r))
                                     (scaling 2 2 1)) _)))
     (translate (flush-top (cuboid rr rr 10)) 0 (* 3/2 r) 13))))

(define-output encoder
  (union
   (flush-top (cuboid 12 12 7))
   (flush-bottom (cylinder 7/2 13/2))
   (translate (spline 0) 0 0 7)
   (translate (flush-top (cuboid 33 16 8/5)) (- 33/2 12) 0 -7)
   (translate (flush-top (cuboid 5/2 13 16)) (+ 33 -12 -5/2) 0 (+ -7 -8/5))))

(define knob-radius 14)
(define knob-width 2)
(define knob-height 14)

(define-output knob
  (let* ((r (- knob-radius knob-width))
         (finger (transform
                  (sphere 9/2)
                  (scaling 1 1 1/2)
                  (translation (* 5/8 r) 0 (+ knob-height knob-width)))))
    (~> (let ((s (/ (- r 1) r)))
          (extrusion (circle r)
                   (translation 0 0 0)
                   (translation 0 0 (- knob-height 4))
                   (transformation-append
                    (translation 0 0 (- knob-height 3))
                    (scaling s s 1))
                   (transformation-append
                    (translation 0 0 knob-height)
                    (scaling s s 1))))
        (flush-bottom _)
        (intersection _ (translate
                         (sphere (* 6 knob-height)) 0 0 (* -5 knob-height)))
        (when~> _
                outer? (minkowski-sum _ (flush-bottom
                                         (cylinder knob-width knob-width)))
                (not outer?)
                (difference _
                            (minkowski-sum (sphere knob-width) finger)
                            (~> (flush-bottom
                                 (cylinder (+ 3 knob-width 1) 13))
                                (apply
                                 union
                                 _
                                 (list-for ((s (iota 12)))
                                   (transform
                                    (cuboid 50 knob-width 13)
                                    (translation 0 0 15/2)
                                    (rotation (* 30 s) 2))))
                                (difference _ (spline 1/5))
                                (translate _ 0 0 (- knob-height 13)))))
        (list-for ((outer? (list #true #false))) _)
        (apply difference _)
        (difference _ finger))))

(define-output knob-sleeve
  (let* ((ε 1/10)
         (δ 0))
    (~> (cylinder (+ knob-radius δ) knob-height)
        (flush-bottom _)
        (intersection _ (translate
                         (sphere (* 6 knob-height)) 0 0 (* -5 knob-height)))
        (minkowski-sum _ (flush-bottom (cylinder knob-width
                                                 knob-width)))
        (difference _ (~> (hull
                           (flush-bottom
                            (cylinder (+ knob-radius ε) (- knob-height 1)))
                           (flush-bottom
                            (cylinder (+ knob-radius ε -1) knob-height)))
                          (translate _ 0 0 (+ ε knob-width -3))))
        (difference _ (cylinder (+ knob-radius ε -1) 50))
        (apply difference
               _
               (list-for ((s (iota 52)))
                 (transform
                  (linear-extrusion (rotate (regular-polygon 3 3/5) -30)
                                    0 (+ knob-height knob-width))
                  (translation (+ knob-radius knob-width δ) 0 0)
                  (rotation (* 360/52 s) 2))))
        (intersection _ (let ((r (+ knob-radius knob-width δ -1/2)))
                          (hull (linear-extrusion (circle (+ 50 r)) 50 50)
                                (point 0 0 (- r)))))
        (intersection _ (let ((r (+ knob-radius knob-width δ -1/2)))
                          (translate
                           (hull (linear-extrusion (circle (+ 50 r)) -50 -50)
                                 (point 0 0 r))
                           0 0 (+ knob-height knob-width -3/2)))))))

(define-output knob-assembly (union knob knob-sleeve))

(define-output mount-washer
  (let* ((h 7/2)
         (g 1/4)
         (r (+ mount-boss-radius g 1/2)))
    (~> (cylinder r (- h 1/2))
        (flush-bottom _)
        (union _ (hull
                  (linear-extrusion (circle r) 5/2 5/2)
                  (flush-bottom (cylinder (+ r 3/2) 1))))
        (minkowski-sum _ (square-pyramid 1 1 1/2))
        (difference _ (~> (countersink 3 3/2 #true)
                          (rotate _ 180 0)
                          (translate _ 0 0 h)))
        (difference _ (linear-extrusion
                       (circle (+ mount-boss-radius g)) -10 1)))))

(define (place-controller part)
   (transform part
              (rotation 90 2)
              (translation (- enclosure-height 35) 44 pcb-boss-height)))

(define (place-port part)
  (~> part
      (translate _ (- 88/5 767/20) -431/20 (+ 8/5 25/2 3/2))
      (place-controller _)))

(define (place-psu part)
   (~> part
       (translate _ (- 35/2 -58/2 767/20 81/8) 63 1)
       (place-controller _)))

(define (place-display part)
  (~> part
      (rotate _ -90 0)
      (translate _
                 (- enclosure-height 5 89/4)
                 enclosure-width
                 (+ (/ enclosure-face-depth 2) (+ -37/2 28/2 1)))))

(define (place-display-boss part)
  (list-for ((θ (list 0 180))
             (s (list -1/2 1/2)))
    (~> part
        (rotate _  180 1)
        (translate _ (* s 79/2) 16 -2)
        (rotate _  θ 2)
        (place-display _))))

(define (place-display-opening part)
  (~> part
      (translate _ 0 (+ -37/2 28/2 1) 0)
      (place-display _)))

(define (place-mount-boss part x)
  (list-for ((s (list 0 1))
             (t (list 0 1)))
    (let* ((δ (λ (x ε)
               (* (- (* x 2) 1)
                  (- (/ wall-width 2) mount-boss-radius ε))))
           (twδ (+ (* t (- enclosure-width enclosure-face-inset)) (δ t 0))))
      (~> part
          (translate _ 0 0 (+ mount-plate-width
                              mount-gasket-width
                              mount-boss-elevation))
          (rotate _ clip-angle 0)
          (translate _
                     (+ (* s enclosure-height) (* (δ s 0) x))
                     twδ
                     (+ (* (+ twδ (* 3/2 wall-width)) (tan-degrees clip-angle))
                        enclosure-depth))))))

(define (place-encoder part)
  (~> part
      (rotate _ -90 0)
      (translate _ 39 (+ enclosure-width -3) (/ enclosure-face-depth 2))))

(define-output display-gasket
  (~> (cuboid 45 38 3)
      (translate _ 0 -1/2 0)
      (minkowski-sum _ (linear-extrusion (regular-polygon 4 4/5) 0))
      (flush-top _)
      (clip _ (transform
               (plane 0 0 1 0)
               (translation 0 0 13)
               (rotation -45 0)))

      (place-display _)
      (difference _ (minkowski-sum
                     (apply cuboid (make-list 3 2/5))
                     (apply
                      union
                      (place-display display)
                      (place-display-boss (display-boss #false)))))))

(define-output pcbs
  (union
   (place-psu psu)
   (place-controller controller)
   (place-display (translate display 0 0 -1/10))
   (place-encoder (rotate encoder 90 2))))

(define (plate w z)
  (~> (cuboid (+ enclosure-height 20)
              (+ (/ enclosure-width (cos-degrees clip-angle)) 20)
              w)
      (flush _ -1 -1 -1)
      (translate _ -10 -10 0)
      (rotate _ clip-angle 0)
      (translate _ 0 0 (+ enclosure-depth z))))

(define (usb-cutout-face δ)
  (let* ((d 3)
         (c 1/2)
         (x (* 2 (- d c))))
    (offset
     (minkowski-sum
      (regular-polygon 4 c)
      (rectangle (+ 10 x) (+ 5 x))) δ)))

(define-output usb-plug
  (~> (let* ((l (+ 35 88/5 -767/20))
             (c 1)
             (ε 3/20)
             (δ (- (+ ε c))))
        (union
         (linear-extrusion (difference
                            (usb-cutout-face δ)
                            (usb-cutout-face (- δ 1/10))) (+ 1/10 wall-width) l)
         (hull
          (linear-extrusion (usb-cutout-face δ) l l)
          (linear-extrusion (usb-cutout-face (+ δ (* 1/3 wall-width)))
                            (+ l (* 1/3 wall-width))
                            (+ l (* 2/3 wall-width))))))
      (minkowski-sum _ (linear-extrusion (regular-polygon 4 1) 0))))

(define-output enclosure
  (~> (if outer?
          (cuboid enclosure-height
                  enclosure-width
                  (* 3 enclosure-depth))

          ;; Apply a bit of chamfering to the inner edges.

          (let* ((c 1)
                 (cc (+ c c)))
            (~> (cuboid (- enclosure-height cc)
                        (- enclosure-width cc)
                        100)
                (minkowski-sum _ (hull
                                  (linear-extrusion (rectangle cc cc) 0)
                                  (point 0 0 (- c)))))))

      (flush _ -1 -1 -1)

      (clip _ (transform
               (plane 0 0 -1 0)
               (rotation -45 0)
               (translation 0 0 5)))

      ;; Front face cut

      (difference
         _
         (let* ((θ (if outer? 90 60))
                (s (sin-degrees θ))
                (c (cos-degrees θ))
                (x (+ enclosure-depth
                      (* (- enclosure-width enclosure-face-inset)
                         (tan-degrees clip-angle))
                      (- enclosure-face-depth)))
                (y (- enclosure-face-inset))
                (r (/ (+ (* x x) (* y y)) (* 2 (+ (* x s) (* y c))))))
           (print-note r)
           (~> (cylinder r enclosure-height)
               (flush-bottom _)
               (rotate _ 90 1)
               (translate _ 0
                          (+ enclosure-width (* r c))
                          (+ enclosure-face-depth (* r s))))))

      ;; Encoder pocket

      (difference _ (~> (hull
                         (linear-extrusion
                          (apply
                           hull
                           (list-for ((t (list -1 1))
                                      (s (list -1 1)))
                             (translate (circle 3/2) (* s 7) (* t 7)))) 0)
                         (linear-extrusion (circle 19) 10 10))
                        (place-encoder _)))

      ;; USB port pocket

      (difference _ (~> (usb-cutout-face wall-width)
                        (linear-extrusion _ 0 50)
                        (rotate _ 90 0)
                        (place-port _)))

      (when~> _ outer? (minkowski-sum _ chamfer-kernel))
      (apply
       difference
       _
       (append
        ;; Controller bosses

        (list-for ((s (list -1/2 1/2))
                   (t (list -1/2 1/2)))
          (~> (thread-boss 32/5 17/5 4 5 outer?)
              (rotate _ 45 2)
              (translate _ (* s 706/10) (* t 371/10) 0)
              (place-controller _)))

        ;; PSU bosses

        (list-for ((s (list -1/2 1/2))
                   (t (list -1/2 1/2)))
          (~> (thread-boss 36/5 4 5 5 outer?)
              (rotate _ 45 2)
              (translate _ (* s 58) (* t 17) 0)
              (place-psu _)))

        ;; MAX31865 bosses

        (list-for ((s (list -1/2 1/2)))
          (~> (thread-boss 36/5 4 (+ pcb-boss-height 8/5 11) 6 outer?)
              (translate _
                         (- 35/2 767/20 (* s 81/4))
                         (+ 431/20 253/10 -5)
                         (+ 8/5 11))
              (place-controller _)))

        ;; Display bosses

        (place-display-boss (display-boss outer?))))

      (when~> _
              ;; Display cutouts

              outer?
              (difference _

                          (~> (rectangle 28 28)
                              (extrusion
                               _
                               (translation 0 0 0)
                               (translation 0 0 (* wall-width 3/5))
                               (transformation-append
                                (translation 0 0 (+ (* wall-width 3/5) 14))
                                (scaling 2 2 2)))
                              (place-display-opening _))

                          (~> (cuboid 171/5 38 10)
                              (translate _ 0 -1/2 0)
                              (flush-top _)
                              (place-display _)))

              ;; Encoder opening cutouts

              outer?
              (difference _ (place-encoder
                             (apply union
                              (cylinder 19/5 20)
                              (list-for ((s (iota 4)))
                                (transform
                                 (cuboid 6/5 11/5 3)
                                 (translation -13/2 0 0)
                                 (rotation (* 90 s) 2)))))))

      (list-for ((outer? (list #true #false))) _)
      (apply difference _)

      (difference
       _
       ;; Front face fluting

       (difference
        (apply
         union
         (list-for ((s (iota (/ enclosure-face-depth 2))))
           (~> (rectangle (- enclosure-height 1) 1/5)
               (linear-extrusion _ 0)
               (minkowski-sum _ (square-pyramid 1 1 1))
               (rotate _ 90 0)
               (translate _
                          (/ enclosure-height 2)
                          (+ enclosure-width wall-width)
                          (+ (* 2 s) 1)))))

        (apply hull
               (list-for ((x (list 5 -5)))
                 (let* ((l 2)
                        (r 21)
                        (rr (+ r r)))
                   (union
                    (hull
                     (~> (linear-extrusion (circle r) 0)
                         (place-encoder _)
                         (scale _ 1 0 1)
                         (translate _
                                    (- x l)
                                    (+ enclosure-width wall-width x)
                                    0))
                     (~> (linear-extrusion (rectangle rr rr) 0)
                         (place-display-opening _)
                         (translate _ (- l x) (+ wall-width x) 0)))))))))


      ;; Ventilation grilles

      (apply difference
             _
             (list-for ((t (list -25 131))
                        (s (iota 10 -9/2)))
               (let ((w 13/5)
                     (δ 11/5)
                     (h (* 2 (+ 20 8/5))))
                 (~> (extrusion (rectangle h 50) (rotation 90 1))
                     (minkowski-sum _ (sphere (/ w 2)))
                     (flush-north _)
                     (translate _ (+ 61/4 (* s (+ w δ))) t 0)
                     (place-controller _)))))

      (clip _ (mount-plane 1))

      ;; Turn the USB port pocket's fillets into chamfers.

      (difference _
             (~> (hull
                  (linear-extrusion (usb-cutout-face 0) 0)
                  (linear-extrusion (usb-cutout-face 60) 50 50))
                 (translate _ 0 0 (+ 35 88/5 -767/20))
                 (rotate _ 90 0)
                 (place-port _)))

      ;; USB port opening

      (difference _ (~> (rectangle 10 5)
                        (extrusion _
                                   (translation 0 0 0)
                                   (translation 0 0 (* wall-width 2/3))
                                   (transformation-append
                                    (translation 0 0 wall-width)
                                    (scaling (+ 1 (/ wall-width 8 3/2))
                                             (+ 1 (/ wall-width 3 3/2))
                                             1)))
                        (rotate _ 90 0)
                        (minkowski-sum _
                                       (~> (regular-polygon 4 1/2)
                                           (extrusion _ (rotation 90 0))))
                        (place-port _)))

      ;; Mounting bosses

      (union _ (apply
                union
                 (map
                  union
                  (~> (cylinder mount-boss-radius mount-boss-height)
                      (flush-top _)
                      (place-mount-boss _ 1))
                  (map
                   hull
                   (~> (linear-extrusion (circle mount-boss-radius)
                                         (- mount-boss-height)
                                         (- mount-boss-height))
                       (place-mount-boss _ 1))
                   (~> (extrusion
                        (circle (/ wall-width 2))
                        (translation 0 0 (* -15/8 mount-boss-height)))
                       (place-mount-boss _ -1/2))))))

      (difference _ (apply
                     union
                     (~> (cylinder 11/5 67/10)
                         (flush-top _)
                         (place-mount-boss _ 1))))))

(define-output mount-plate
  (apply difference
         (plate mount-plate-width 1)
         (place-mount-boss (cylinder (+ mount-boss-radius 1/5) 50) 1)))

(define-output mount-gasket
  (let ((r 7/5)
        (d 3/10) ; depth of mating inset
        (ε 1/5)  ; dilation of mating inset
        (c 2/5))
    (~> (plate 1/100 -1/100)
        (intersection _ enclosure)

        (rotate _ (- clip-angle) 0)
        (λ (part)
          (let ((kernel
                 (λ (h z)
                   (hull
                    (flush
                     (cylinder (- r c 1/100)
                               (* h mount-gasket-width)) 0 0 z)
                    (flush
                     (cylinder (- r 1/100)
                               (- (* h mount-gasket-width) c)) 0 0 z)))))
            (difference
             (union
              (minkowski-sum
               (kernel 3 1)
               (difference
                (hull part)
                (minkowski-sum part (cylinder 3 1))))
              (minkowski-sum (hull part) (rotate (kernel 1 -1) 180 0)))
             (minkowski-sum part
                            (translate (cylinder ε (+ d d))
                                       0 0 (- mount-gasket-width))))))
        (translate _ 0 0 (- mount-gasket-width d -1/10))
        (apply difference
               _
               (list-for ((s (list -1 1)))
                 (translate (cylinder 5 50)
                            (- enclosure-height 35)
                            (+ (* enclosure-depth (sin-degrees clip-angle))
                               (/ (- enclosure-width enclosure-face-inset)
                                  (cos-degrees clip-angle)
                                  2)
                               (* 15 s))
                            enclosure-depth)))
        (rotate _ clip-angle 0)

        (apply difference
               _
               (place-mount-boss (cylinder (+ mount-boss-radius ε) 50) 1)))))

(define-option debug? #false)
(define-option depth 0)

(define-output assembly
  ((if debug? intersection union)
   (union enclosure
          (~> knob-assembly
              (rotate _ 90 2)
              (translate _ 0 0 (+ wall-width 4))
              (place-encoder _))
          (~> usb-plug
              (rotate _ 90 0)
              (place-port _))
          (~> mount-washer
              (translate _ 0 0 mount-boss-elevation)
              (place-mount-boss _ 1)
              (apply union _)))

   (union pcbs
          display-gasket
          (~> nut
              (translate _ 0 0 wall-width)
              (place-encoder _))
          mount-gasket
          mount-plate)))

(define-output section
  (clip assembly (plane -1 0 0 depth)))
