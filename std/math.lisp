;;; math.lisp - Numerical and mathematical utilities

(defconst PI  3.141592653589793)
(defconst E   2.718281828459045)
(defconst PHI 1.618033988749895)

(defun 1+ (x)
  (assert (number? x) "X must be number")
  (+ x 1))

(defun 1- (x)
  (assert (number? x) "X must be number")
  (- x 1))

(defun even? (x)
  (assert (int? x) "X must be integer")
  (= (% x 2) 0))

(defun odd? (x)
  (assert (int? x) "X must be integer")
  (not (even? x)))

(defun abs (x)
  (if (< x 0)
      (- 0 x)
      x))

(defun min (a b)
  (if (< a b)
      a
      b))

(defun max (a b)
  (if (> a b)
      a
      b))

(defun clamp (x low high)
  "Return LOW if X < LOW, HIGH if X > HIGH, X if LOW <= X <= HIGH."
  (cond
    ((< x low)  low)
    ((> x high) high)
    (else       x)))

(defun factorial (x)
  (assert (and (int? x) (>= x 0)) "X must be non-negative integer")
  (if (= x 0)
      1
      (* x (factorial (1- x)))))

(defun cbrt (x)
  (assert (number? x) "X must be number")
  (copysign (pow (abs x) (/ 1.0 3.0)) x))

(defun deg->rad (deg)
  "Convert degrees to radians."
  (assert (number? deg) "DEG must be number")
  (* deg (/ PI 180.0)))

(defun rad->deg (rad)
  "Convert radians to degrees."
  (assert (number? rad) "RAD must be number")
  (* rad (/ 180.0 PI)))

(defun trunc (x)
  (assert (number? x) "X must be number")
  (float (int x)))

(defun signum (x)
  "-1 if X < 0, 0 if X = 0, 1 if X > 0"
  (assert (number? x) "X must be number")
  (cond
    ((< x 0) -1)
    ((= x 0)  0)
    ((> x 0)  1)))

(defun rem (a b)
  (assert (and (number? a) (number? b)) "A and B must be numbers")
  (- a (* b (trunc (/ a b)))))

(defun mod (a b)
  (assert (and (number? a) (number? b)) "A and B must be numbers")
  (- a (* b (floor (/ a b)))))

(defun approx= (a b &optional (rel-tol 0.0000001) (abs-tol 0.000000001))
  "Returns t if A is approximately equal to B."
  (assert (and (number? a) (number? b)) "Both A and B must be numbers")
  (if (= a b)
      t
      (let ((diff (abs (- a b)))
            (tolerance (max abs-tol (* rel-tol (max (abs a) (abs b))))))
        (<= diff tolerance))))

(defun gcd (a b)
  (assert (and (number? a) (number? b)) "A and B must be numbers")
  (let ((x (abs (int a)))
        (y (abs (int b))))
    (if (= y 0)
        x
        (gcd y (% x y)))))

(defun lcm (a b)
  (assert (and (number? a) (number? b)) "A and B must be numbers")
  (if (or (= a 0) (= b 0))
      0
      (abs (* (/ (int a) (gcd a b)) (int b)))))

(defun hypot (x y)
  (assert (and (number? x) (number? y)) "X and Y must be numbers")
  (let ((ax (abs (float x)))
        (ay (abs (float y))))
    (cond
      ((= ax 0.0) ay)
      ((= ay 0.0) ax)
      (t
       (let ((max-val (max ax ay))
             (min-val (min ax ay)))
         (let ((ratio (/ min-val max-val)))
           (* max-val (sqrt (+ 1.0 (* ratio ratio))))))))))

(defun lerp (a b t-val)
  (assert (and (number? a) (number? b) (number? t-val)) "All arguments must be numbers")
  (let ((ft (* 1.0 t-val)))
    (+ (* (- 1.0 ft) a) (* ft b))))

(defconst float= approx=)
(defconst feq approx=)
(defconst isclose approx=)

;; Trigonometry

(defun tan (x)
  (assert (number? x) "X must be number")
  (/ (sin x) (cos x)))

(defun cot (x)
  (assert (number? x) "X must be number")
  (/ 1.0 (tan x)))

(defun sec (x)
  (assert (number? x) "X must be number")
  (/ 1.0 (cos x)))

(defun csc (x)
  (assert (number? x) "X must be number")
  (/ 1.0 (sin x)))

(defun asin (x)
  (assert (and (number? x) (<= (abs x) 1.0)) "X must be in range [-1, 1]")
  (atan2 x (sqrt (- 1.0 (* x x)))))

(defun acos (x)
  (assert (and (number? x) (<= (abs x) 1.0)) "X must be in range [-1, 1]")
  (atan2 (sqrt (- 1.0 (* x x))) x))

(defun atan (x)
  (assert (number? x) "X must be number")
  (atan2 x 1.0))

(defun acot (x)
  (assert (number? x) "X must be number")
  (atan2 1.0 x))

(defun asec (x)
  (assert (and (number? x) (>= (abs x) 1.0)) "X must satisfy |x| >= 1")
  (acos (/ 1.0 x)))

(defun acsc (x)
  (assert (and (number? x) (>= (abs x) 1.0)) "X must satisfy |x| >= 1")
  (asin (/ 1.0 x)))

(defun sinh (x)
  (assert (number? x) "X must be number")
  (/ (- (exp x) (exp (- x))) 2))

(defun cosh (x)
  (assert (number? x) "X must be number")
  (/ (+ (exp x) (exp (- x))) 2))

(defun tanh (x)
  (assert (number? x) "X must be number")
  (/ (sinh x) (cosh x)))

(defun coth (x)
  (assert (and (number? x) (not (= x 0))) "X must be non-zero number")
  (/ (cosh x) (sinh x)))

(defun sech (x)
  (assert (number? x) "X must be number")
  (/ 1 (cosh x)))

(defun csch (x)
  (assert (number? x) "X must be number")
  (/ 1 (sinh x)))

(defun asinh (x)
  (assert (number? x) "X must be number")
  (log (+ x (sqrt (+ (* x x) 1)))))

(defun acosh (x)
  (assert (and (number? x) (>= x 1)) "X must be >= 1")
  (log (+ x (sqrt (- (* x x) 1)))))

(defun atanh (x)
  (assert (and (number? x) (< (abs x) 1.0)) "X must be in range (-1, 1)")
  (* 0.5 (log (/ (+ 1 x) (- 1 x)))))

(defun acoth (x)
  (assert (and (number? x) (> (abs x) 1.0)) "X must satisfy |x| > 1")
  (* 0.5 (log (/ (+ x 1) (- x 1)))))

(defun asech (x)
  (assert (and (number? x) (> x 0) (<= x 1)) "X must be in range (0, 1]")
  (log (/ (+ 1 (sqrt (- 1 (* x x)))) x)))

(defun acsch (x)
  (assert (and (number? x) (not (= x 0))) "X must be non-zero number")
  (log (+ (/ 1 x) (/ (sqrt (+ 1 (* x x))) (abs x)))))
