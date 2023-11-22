package main

import (
	"fmt"
	"log"
	"net/http"
	"os"
	"runtime"
	"strings"
)

const VERSION = "1.0.0"

type StatusRecorder struct {
	http.ResponseWriter
	status int
}

var be_quiet bool = false

func getenv(key, fallback string) string {
	value := os.Getenv(key)
	if len(value) == 0 {
		return fallback
	}
	return value
}

func ping(w http.ResponseWriter, r *http.Request) {
	fmt.Fprint(w, "PONG")
}

func version(w http.ResponseWriter, r *http.Request) {
	fmt.Fprint(w, VERSION)
}

func (r *StatusRecorder) WriteHeader(status int) {
	r.status = status
	r.ResponseWriter.WriteHeader(status)
}

func serviceHandler(h http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		recorder := &StatusRecorder{
			ResponseWriter: w,
			status:         200,
		}
		h.ServeHTTP(recorder, r)
		if !be_quiet {
			ip := strings.Split(r.RemoteAddr, ":")
			log.Println("config-service", ip[0], recorder.status, r.Method, r.URL.Path)
		}
	})
}

func main() {
	dir := getenv("DISTRIBUTE", "/config")
	env_quiet := getenv("CONFIG_SERVICE_QUIET", "no")
	if env_quiet != "no" {
		be_quiet = true
	}
	listen := getenv("CONFIG_SERVICE_LISTEN", "0.0.0.0:1236")
	fs := http.FileServer(http.Dir(dir))
	mux := http.NewServeMux()
	mux.Handle("/cfg/", http.StripPrefix("/cfg", fs))
	mux.HandleFunc("/.ping", ping)
	mux.HandleFunc("/.version", version)
	fmt.Printf("CONFIG SERVIVCE VERSION %v READY AT %v ON %v/%v\n", VERSION, listen, runtime.GOOS, runtime.GOARCH)
	if banner, err := os.ReadFile("/.banner"); err == nil {
		fmt.Print(string(banner))
	}
	http.ListenAndServe(listen, serviceHandler(mux))
}
