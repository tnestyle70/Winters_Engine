package matchmaking

import (
	"net/http"

	"github.com/go-chi/chi/v5"
	"winters-backend/pkg/middleware"
	"winters-backend/pkg/response"
)

type Handler struct {
	svc *Service
}

func NewHandler(svc *Service) *Handler {
	return &Handler{svc: svc}
}

func (h *Handler) Routes() chi.Router {
	r := chi.NewRouter()
	r.Post("/join", h.Join)
	r.Delete("/leave", h.Leave)
	r.Get("/status", h.Status)
	return r
}

func (h *Handler) Join(w http.ResponseWriter, r *http.Request) {
	claims := middleware.GetClaims(r.Context())
	if claims == nil {
		response.Error(w, http.StatusUnauthorized, "missing claims")
		return
	}

	if err := h.svc.Join(r.Context(), claims.UserID); err != nil {
		response.Error(w, http.StatusConflict, err.Error())
		return
	}
	response.JSON(w, http.StatusOK, map[string]string{"status": "queued"})
}

func (h *Handler) Leave(w http.ResponseWriter, r *http.Request) {
	claims := middleware.GetClaims(r.Context())
	if claims == nil {
		response.Error(w, http.StatusUnauthorized, "missing claims")
		return
	}

	if err := h.svc.Leave(r.Context(), claims.UserID); err != nil {
		response.Error(w, http.StatusInternalServerError, err.Error())
		return
	}
	response.JSON(w, http.StatusOK, map[string]string{"status": "left"})
}

func (h *Handler) Status(w http.ResponseWriter, r *http.Request) {
	claims := middleware.GetClaims(r.Context())
	if claims == nil {
		response.Error(w, http.StatusUnauthorized, "missing claims")
		return
	}

	status, err := h.svc.Status(r.Context(), claims.UserID)
	if err != nil {
		response.Error(w, http.StatusInternalServerError, err.Error())
		return
	}
	response.JSON(w, http.StatusOK, status)
}
